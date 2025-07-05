#include "config.h"

#include "torrent/connection_manager.h"

#include "net/listen.h"
#include "rak/socket_address.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

ConnectionManager::ConnectionManager() :
  m_bindAddress((new rak::socket_address())->c_sockaddr()),
  m_localAddress((new rak::socket_address())->c_sockaddr()),
  m_proxyAddress((new rak::socket_address())->c_sockaddr()),
  m_listen(new Listen) {

  rak::socket_address::cast_from(m_bindAddress)->clear();
  rak::socket_address::cast_from(m_localAddress)->clear();
  rak::socket_address::cast_from(m_proxyAddress)->clear();
}

ConnectionManager::~ConnectionManager() {
  delete m_listen;

  delete m_bindAddress;
  delete m_localAddress;
  delete m_proxyAddress;
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

void
ConnectionManager::set_bind_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a bind address that is not an af_inet address.");

  rak::socket_address::cast_from(m_bindAddress)->copy(*rsa, rsa->length());
}

void
ConnectionManager::set_local_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a local address that is not an af_inet address.");

  rak::socket_address::cast_from(m_localAddress)->copy(*rsa, rsa->length());
}

void
ConnectionManager::set_proxy_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a proxy address that is not an af_inet address.");

  rak::socket_address::cast_from(m_proxyAddress)->copy(*rsa, rsa->length());
}

uint32_t
ConnectionManager::filter(const sockaddr* sa) {
  if (m_block_ipv4 && sa_is_inet(sa))
    return 0;

  if (m_block_ipv6 && sa_is_inet6(sa))
    return 0;

  if (m_block_ipv4in6 && sa_is_v4mapped(sa))
    return 0;

  if (m_slot_filter)
    return m_slot_filter(sa);

  return 1;
}

bool
ConnectionManager::listen_open(port_type begin, port_type end) {
  if (!m_listen->open(begin, end, m_listen_backlog, rak::socket_address::cast_from(m_bindAddress)))
    return false;

  m_listen_port = m_listen->port();

  return true;
}

void
ConnectionManager::listen_close() {
  m_listen->close();
}

void
ConnectionManager::set_listen_backlog(int v) {
  if (v < 1 || v >= (1 << 16))
    throw input_error("backlog value out of bounds");

  if (m_listen->is_open())
    throw input_error("backlog value must be set before listen port is opened");

  m_listen_backlog = v;
}

} // namespace torrent
