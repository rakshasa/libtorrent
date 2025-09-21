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

void
ConnectionManager::set_send_buffer_size(uint32_t s) {
  m_sendBufferSize = s;
}

void
ConnectionManager::set_receive_buffer_size(uint32_t s) {
  m_receiveBufferSize = s;
}

void
ConnectionManager::set_encryption_options(uint32_t options) {
  m_encryptionOptions = options;
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
  auto bind_address = config::network_config()->bind_address();

  switch (bind_address->sa_family) {
  case AF_UNSPEC:
    if (config::network_config()->is_block_ipv6())
      bind_address = sa_make_inet_any();
    else
      bind_address = sa_make_inet6_any();

    break;
  case AF_INET:
  case AF_INET6:
    break;
  default:
    throw input_error("invalid bind address family");
  }

  // TODO: Add blocking of ipv4 on socket level.

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
