#include "config.h"

#include "torrent/connection_manager.h"

#include "net/listen.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/network_config.h"

namespace torrent {

ConnectionManager::ConnectionManager() :
  m_listen(new Listen) {
}

ConnectionManager::~ConnectionManager() {
  delete m_listen;
}

bool
ConnectionManager::can_connect() const {
  return m_size < m_maxSize;
}

uint32_t
ConnectionManager::filter(const sockaddr* sa) {
  if (config::network_config()->is_block_ipv4() && sa_is_inet(sa))
    return 0;

  if (config::network_config()->is_block_ipv6() && sa_is_inet6(sa))
    return 0;

  if (config::network_config()->is_block_ipv4in6() && sa_is_v4mapped(sa))
    return 0;

  if (m_slot_filter)
    return m_slot_filter(sa);

  return 1;
}

bool
ConnectionManager::is_listen_open() const {
  return m_listen->is_open();
}

bool
ConnectionManager::listen_open(port_type begin, port_type end) {
  // TODO: Make this a helper function in NetworkConfig.
  auto bind_address = config::network_config()->bind_address_or_any_and_null();

  // TODO: Fix this when we properly handle block ipv4/6.
  if (bind_address == nullptr)
    throw internal_error("Could not find a valid bind address to open listen socket.");

  if (!m_listen->open(begin, end, config::network_config()->listen_backlog(), bind_address.get()))
    return false;

  config::network_config()->set_listen_port(m_listen->port());

  return true;
}

void
ConnectionManager::listen_close() {
  m_listen->close();
}

void
ConnectionManager::set_listen_backlog(int v) {
  if (m_listen->is_open())
    throw input_error("backlog value must be set before listen port is opened");

  config::network_config()->set_listen_backlog(v);
}

} // namespace torrent
