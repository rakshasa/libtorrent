#include "config.h"

#include <sys/types.h>

#include <rak/address_info.h>
#include <rak/socket_address.h>

#include "net/listen.h"

#include "connection_manager.h"
#include "error.h"
#include "exceptions.h"
#include "manager.h"

namespace torrent {

// Fix TrackerUdp, etc, if this is made async.
static ConnectionManager::slot_resolver_result_type*
resolve_host(const char* host, int family, int socktype, ConnectionManager::slot_resolver_result_type slot) {
  if (manager->thread_main()->is_current())
    thread_base::release_global_lock();

  rak::address_info* ai;
  int err;

  if ((err = rak::address_info::get_address_info(host, family, socktype, &ai)) != 0) {
    if (manager->thread_main()->is_current())
      thread_base::acquire_global_lock();

    slot(NULL, err);
    return NULL;
  }

  rak::socket_address sa;
  sa.copy(*ai->address(), ai->length());
  rak::address_info::free_address_info(ai);

  if (manager->thread_main()->is_current())
    thread_base::acquire_global_lock();

  slot(sa.c_sockaddr(), 0);
  return NULL;
}

ConnectionManager::ConnectionManager() :
  m_size(0),
  m_maxSize(0),

  m_priority(iptos_throughput),
  m_sendBufferSize(0),
  m_receiveBufferSize(0),
  m_encryptionOptions(encryption_none),

  m_listen(new Listen),
  m_listen_port(0),
  m_listen_backlog(SOMAXCONN),

  m_block_ipv4(false),
  m_block_ipv6(false),
  m_prefer_ipv6(false) {

  m_bindAddress = (new rak::socket_address())->c_sockaddr();
  m_localAddress = (new rak::socket_address())->c_sockaddr();
  m_proxyAddress = (new rak::socket_address())->c_sockaddr();

  rak::socket_address::cast_from(m_bindAddress)->clear();
  rak::socket_address::cast_from(m_localAddress)->clear();
  rak::socket_address::cast_from(m_proxyAddress)->clear();

  m_slot_resolver = std::bind(&resolve_host,
                              std::placeholders::_1,
                              std::placeholders::_2,
                              std::placeholders::_3,
                              std::placeholders::_4);
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
#ifdef USE_OPENSSL
  m_encryptionOptions = options;
#else
  throw input_error("Compiled without encryption support.");
#endif
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
  if (!m_slot_filter)
    return 1;
  else
    return m_slot_filter(sa);
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

}
