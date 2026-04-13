#include "config.h"

#include "dht/dht_transaction.h"

#include <cassert>

#include "dht/dht_bucket.h"
#include "torrent/exceptions.h"
#include "torrent/object_stream.h"
#include "torrent/net/socket_address.h"
#include "tracker/tracker_dht.h"

namespace torrent {

template<>
const DhtMessage::key_list_type DhtMessage::base_type::keys;

//
// DhtTransactionPacket:
//

DhtTransactionPacket::DhtTransactionPacket(const sockaddr* s, const DhtMessage& d, unsigned int id, std::shared_ptr<DhtTransaction> t)
  : m_socket_address(sa_copy(s)),
    m_id(id),
    m_transaction(std::move(t)) {

  build_buffer(d);
}

DhtTransactionPacket::DhtTransactionPacket(const sockaddr* s, const DhtMessage& d)
  : m_socket_address(sa_copy(s)),
    m_id(-this_thread::cached_seconds().count()) {

  build_buffer(d);
}

void
DhtTransactionPacket::build_buffer(const DhtMessage& msg) {
  char buffer[1500];  // If the message would exceed an Ethernet frame, something went very wrong.
  object_buffer_t result = static_map_write_bencode_c(object_write_to_buffer, NULL, std::make_pair(buffer, buffer + sizeof(buffer)), msg);

  m_length = result.second - buffer;
  m_data   = std::make_unique<char[]>(m_length);

  memcpy(m_data.get(), buffer, m_length);
}

DhtTransaction::DhtTransaction(int quick_timeout, int timeout, const HashString& id, const sockaddr* sa)
  : m_id(id),
    m_hasQuickTimeout(quick_timeout > 0),
    m_socket_address(sa_copy(sa)),
    m_timeout(this_thread::cached_seconds().count() + timeout),
    m_quickTimeout(this_thread::cached_seconds().count() + quick_timeout) {
}

DhtTransaction::~DhtTransaction() {
  if (m_packet != NULL)
    m_packet->set_failed();
}

DhtTransaction::key_type
DhtTransaction::key(const sockaddr* sa, int id) {
  if (sa_is_inet(sa))
    return (static_cast<uint64_t>(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr) << 32) + id;
  else if (sa_is_inet6(sa))
    throw internal_error("DhtTransaction::key() called with inet6 address.");
  else
    throw internal_error("DhtTransaction::key() called with non-inet address.");
}

bool
DhtTransaction::key_match(key_type key, const sockaddr* sa) {
  if (sa_is_inet(sa))
    return (key >> 32) == static_cast<uint64_t>(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr);
  else if (sa_is_inet6(sa))
    throw internal_error("DhtTransaction::key_match() called with inet6 address.");
  else
    throw internal_error("DhtTransaction::key_match() called with non-inet address.");
}

//
// DhtTransactionSearch:
//

DhtTransactionSearch::DhtTransactionSearch(int quick_timeout, int timeout, dht::DhtSearch::const_accessor& node)
  : DhtTransaction(quick_timeout, timeout, node.node()->id(), node.node()->address()),
    m_node(node),
    m_search(node.search()) {

  if (!m_hasQuickTimeout)
    m_search->m_concurrency++;
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

  if (m_node.search() != m_search)
    throw internal_error("DhtTransactionSearch::complete() called for node from wrong search.");

  if (!m_hasQuickTimeout)
    m_search->m_concurrency--;

  m_search->node_status(m_node.node(), success);
  m_node = m_search->end();
}

DhtTransactionSearch::~DhtTransactionSearch() {
  if (m_node != m_search->end())
    complete(false);

  if (m_search->complete())
    m_search.reset(); // TODO: We to have a more strict ownership model for DhtSearch and DhtTransactionSearch.
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
