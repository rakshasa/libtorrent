#include "config.h"

#include "dht/dht_node.h"

#include "torrent/object.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "net/address_list.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_hash(torrent::LOG_DHT_NODE, this->id(), "dht_node", log_fmt, __VA_ARGS__);

namespace torrent {

DhtNode::DhtNode(const HashString& id, const sockaddr* sa)
  : HashString(id),
    m_socket_address(sa_copy(sa)) {

  // TODO: Change this to use the id hash similar to how peer info
  // hash'es are logged.
  LT_LOG_THIS("created node : %s", sa_pretty_str(sa).c_str());

  // if (sa->family() != AF_INET &&
  //     (sa->family() != AF_INET6 || !sa->sa_inet6()->is_any()))
  //   throw resource_error("Address not af_inet or in6addr_any");
}

DhtNode::DhtNode(const std::string& id, const Object& cache)
  : HashString(*HashString::cast_from(id.c_str())),
    m_last_seen(cache.get_key_value("t")) {

  // TODO: Check how DHT handles inet6.
  m_socket_address = sa_make_inet_h(cache.get_key_value("i"), cache.get_key_value("p"));

  LT_LOG_THIS("initializing node : %s", sap_pretty_str(m_socket_address).c_str());

  update();
}

void
DhtNode::set_address(const sockaddr* sa) {
  m_socket_address = sa_copy(sa);
}

char*
DhtNode::store_compact(char* buffer) const {
  HashString::cast_from(buffer)->assign(data());

  if (m_socket_address->sa_family != AF_INET)
    throw internal_error("DhtNode::store_compact called with non-inet address.");

  auto sin = reinterpret_cast<sockaddr_in*>(m_socket_address.get());

  SocketAddressCompact compact(sin);
  std::memcpy(buffer + 20, compact.c_str(), 6);

  return buffer + 26;
}

Object*
DhtNode::store_cache(Object* container) const {
  if (m_socket_address->sa_family == AF_INET6) {
    // Currently, all we support is in6addr_any (checked in the constructor),
    // which is effectively equivalent to this. Note that we need to specify
    // int64_t explicitly here because a zero constant is special in C++ and
    // thus we need an explicit match.
    container->insert_key("i", int64_t{0});
    container->insert_key("p", sap_port(m_socket_address));

  } else if (m_socket_address->sa_family == AF_INET) {
    auto sin = reinterpret_cast<sockaddr_in*>(m_socket_address.get());

    container->insert_key("i", ntohl(sin->sin_addr.s_addr));
    container->insert_key("p", ntohs(sin->sin_port));

  } else {
    throw internal_error("DhtNode::store_cache called with non-inet/inet6 address.");
  }

  container->insert_key("t", m_last_seen);
  return container;
}

} // namespace torrent
