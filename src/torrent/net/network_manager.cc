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

// TODO: Currently only opens one listen socket, either ipv4 or ipv6 based on bind address.
// TODO: Log here
// TODO: Verify various combinations of addresses/block_ipv4in6 match tcp46/udp46 sockets

bool
NetworkManager::listen_open(uint16_t begin, uint16_t end) {
  auto guard        = lock_guard();
  auto config_guard = config::network_config()->lock_guard();

  // TODO: Check flag in NetworkConfig instead.
  if (m_listen_inet->is_open() || m_listen_inet6->is_open())
    throw internal_error("NetworkManager::open_listen(): Tried to open listen socket when one is already open.");

  auto [inet_address, inet6_address, block_ipv4in6] = config::network_config()->listen_addresses_unsafe();

  if (inet_address == nullptr && inet6_address == nullptr)
    throw input_error("Could not find a valid bind address to open listen socket.");

  // Add duel-opener to Listen.
  if (inet_address != nullptr && inet6_address != nullptr)
    throw input_error("Opening both ipv4 and ipv6 listen sockets is not yet supported.");

  // TODO: Remember block_ipv4in6.

  if (inet_address != nullptr) {
    if (!Listen::open_single(m_listen_inet.get(), begin, end, m_listen_backlog, inet_address.get()))
      return false;

    config::network_config()->set_listen_port_unsafe(m_listen_inet->port());
    return true;
  }

  if (inet6_address != nullptr) {
    if (!Listen::open_single(m_listen_inet6.get(), begin, end, m_listen_backlog, inet6_address.get()))
      return false;

    config::network_config()->set_listen_port_unsafe(m_listen_inet6->port());
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

int
NetworkManager::listen_backlog() const {
  auto guard = lock_guard();
  return m_listen_backlog;
}

void
NetworkManager::set_listen_backlog(int backlog) {
  if (backlog < 1)
    throw input_error("Tried to set a listen backlog less than 1.");

  if (backlog > (1 << 16))
    throw input_error("Tried to set a listen backlog greater than 65536.");

  auto guard = lock_guard();
  m_listen_backlog = backlog;
}

} // namespace torrent::net
