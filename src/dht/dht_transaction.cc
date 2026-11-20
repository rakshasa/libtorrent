#include "config.h"

#include "dht/dht_transaction.h"

#include <cassert>

#include "dht/dht_bucket.h"
#include "dht/dht_server.h"
#include "torrent/exceptions.h"
#include "torrent/object_stream.h"
#include "torrent/net/socket_address.h"
#include "tracker/tracker_dht.h"

namespace torrent {

template<>
const DhtMessage::key_list_type DhtMessage::base_type::keys;

DhtTransaction::DhtTransaction(int quick_timeout, int timeout, const HashString& id, const sockaddr* sa)
  : m_id(id),
    m_hasQuickTimeout(quick_timeout > 0),
    m_socket_address(sa_copy(sa)),
    m_timeout(this_thread::cached_seconds().count() + timeout),
    m_quickTimeout(this_thread::cached_seconds().count() + quick_timeout) {
}

DhtTransaction::~DhtTransaction() = default;

// The upper 32 bits name the node's address, and the lower 32 bits hold its transaction ID.
uint64_t
dht_transaction_addr_key(const sockaddr* sa) {
  if (sa_is_inet(sa))
    return reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr;

  if (!sa_is_inet6(sa))
    throw internal_error("DhtTransaction::key() called with non-inet address.");

  auto bytes = reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr);
  uint32_t hash = 2166136261u;

  for (int i = 0; i < 16; i++)
    hash = (hash ^ bytes[i]) * 16777619u;

  return hash;
}

DhtTransaction::key_type
DhtTransaction::key(const sockaddr* sa, int id) {
  if (id < 0 || id > 0xffff)
    throw internal_error("DhtTransaction::key() called with an out-of-range two-byte ID.");

  char bytes[] = {static_cast<char>(id >> 8), static_cast<char>(id)};
  return key(sa, bytes, sizeof(bytes));
}

DhtTransaction::key_type
DhtTransaction::key(const sockaddr* sa, const char* id, size_t length) {
  if (id == nullptr || length == 0 || length > 4)
    throw internal_error("DhtTransaction::key() called with an invalid transaction ID.");

  uint32_t value{};

  for (size_t i = 0; i < length; ++i)
    value = (value << 8) | static_cast<unsigned char>(id[i]);

  return (dht_transaction_addr_key(sa) << 32) | value;
}

bool
DhtTransaction::key_match(key_type key, const sockaddr* sa) {
  return (key >> 32) == dht_transaction_addr_key(sa);
}

//
// DhtTransactionSearch:
//

DhtTransactionSearch::DhtTransactionSearch(int quick_timeout, int timeout, std::shared_ptr<dht::DhtSearch> search, dht::DhtSearch::const_accessor& node)
  : DhtTransaction(quick_timeout, timeout, node.node()->id(), node.node()->address()),
    m_node(node),
    m_search(std::move(search)) {

  if (!m_hasQuickTimeout)
    m_search->m_concurrency++;
}

DhtTransactionSearch::~DhtTransactionSearch() {
  if (m_node != m_search->end())
    complete(false);

  m_search->server()->check_search_completed(std::move(m_search));
}

void
DhtTransactionSearch::set_stalled() {
  if (!m_hasQuickTimeout)
    throw internal_error("DhtTransactionSearch::set_stalled() called on already stalled transaction.");

  m_hasQuickTimeout = false;
  m_search->m_concurrency++;
}

void
DhtTransactionSearch::complete(bool success) {
  if (m_node == m_search->end())
    throw internal_error("DhtTransactionSearch::complete() called multiple times.");

  if (!m_hasQuickTimeout)
    m_search->m_concurrency--;

  m_search->node_status(m_node.node(), success);
  m_node = m_search->end();
}

DhtTransaction::transaction_type
DhtTransactionPing::type() const {
  return DHT_PING;
}

DhtTransaction::transaction_type
DhtTransactionFindNode::type() const {
  return DHT_FIND_NODE;
}

DhtTransaction::transaction_type
DhtTransactionGetPeers::type() const {
  return DHT_GET_PEERS;
}

DhtTransaction::transaction_type
DhtTransactionAnnouncePeer::type() const {
  return DHT_ANNOUNCE_PEER;
}

} // namespace torrent
