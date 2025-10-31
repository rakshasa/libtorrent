#include "config.h"

#include "torrent/net/network_manager.h"

#include "net/listen.h"
#include "torrent/exceptions.h"
#include "torrent/net/network_config.h"
#include "torrent/utils/log.h"

#define LT_LOG_NOTICE(log_fmt, ...)                                     \
  lt_log_print_subsystem(LOG_NOTICE, "net::network_manager", log_fmt, __VA_ARGS__);

namespace torrent::net {

NetworkManager::NetworkManager()
  : m_listen_inet(new Listen),
    m_listen_inet6(new Listen) {
}

NetworkManager::~NetworkManager() = default;

bool
NetworkManager::is_listening() const {
  auto guard = lock_guard();
  return m_listen_inet->is_open() || m_listen_inet6->is_open();
}

// TODO: Currently only opens one listen socket, either ipv4 or ipv6 based on bind address.
// TODO: Log here
// TODO: Verify various combinations of addresses/block_ipv4in6 match tcp46/udp46 sockets

// TODO: Fix DHT

bool
NetworkManager::listen_open(uint16_t begin, uint16_t end) {
  auto guard        = lock_guard();
  auto config_guard = config::network_config()->lock_guard();
  auto backlog      = config::network_config()->listen_backlog();

  if (m_listen_inet->is_open() || m_listen_inet6->is_open())
    throw internal_error("NetworkManager::open_listen(): Tried to open listen socket when one is already open.");

  auto [inet_address, inet6_address, block_ipv4in6] = config::network_config()->listen_addresses_unsafe();

  if (inet_address == nullptr && inet6_address == nullptr)
    throw input_error("Neither IPv4 nor IPv6 listen address are suitable for opening listen sockets, check block_ipv4 and block_ipv6 settings.");

  if (inet_address != nullptr && inet6_address != nullptr) {
    if (!Listen::open_both(m_listen_inet.get(), m_listen_inet6.get(), inet_address.get(), inet6_address.get(),
                           begin, end, backlog, block_ipv4in6))
      return false;

    m_listen_port = m_listen_inet->port();
    return true;
  }

  if (inet_address != nullptr) {
    if (!Listen::open_single(m_listen_inet.get(), inet_address.get(), begin, end, backlog, false))
      return false;

    m_listen_port = m_listen_inet->port();
    return true;
  }

  if (inet6_address != nullptr) {
    if (!Listen::open_single(m_listen_inet6.get(), inet6_address.get(), begin, end, backlog, block_ipv4in6))
      return false;

    m_listen_port = m_listen_inet6->port();
    return true;
  }

  throw internal_error("NetworkManager::open_listen(): reached unreachable code.");
}

void
NetworkManager::listen_close() {
  auto guard = lock_guard();

  m_listen_inet->close();
  m_listen_inet6->close();
}

uint16_t
NetworkManager::listen_port() const {
  auto guard = lock_guard();
  return m_listen_port;
}

uint16_t
NetworkManager::listen_port_or_throw() const {
  auto guard = lock_guard();

  if (m_listen_port == 0)
    throw input_error("Tried to get listen port but it is not set.");

  return m_listen_port;
}

} // namespace torrent::net
