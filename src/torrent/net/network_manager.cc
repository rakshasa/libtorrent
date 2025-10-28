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

bool
NetworkManager::listen_open(uint16_t begin, uint16_t end) {
  auto guard = lock_guard();

  // TODO: Check flag in NetworkConfig instead.
  if (m_listen_inet->is_open() || m_listen_inet6->is_open())
    throw internal_error("NetworkManager::open_listen(): Tried to open listen socket when one is already open.");

  auto bind_address = config::network_config()->bind_address_or_any_and_null();

  // TODO: Move up
  auto config_guard = config::network_config()->lock_guard();

  // TODO: Fix this when we properly handle block ipv4/6.
  if (bind_address == nullptr)
    throw internal_error("Could not find a valid bind address to open listen socket.");

  // TODO: Move range check here so we can properly attempt both ipv4 and ipv6.

  if (!open_listen_on_address(bind_address, begin, end))
    return false;

  return true;
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

bool
NetworkManager::open_listen_on_address(c_sa_shared_ptr& bind_address, uint16_t begin, uint16_t end) {
  switch (bind_address->sa_family) {
  case AF_INET:
    if (!m_listen_inet->open(begin, end, m_listen_backlog, bind_address.get()))
      return false;

    config::network_config()->set_listen_port_unsafe(m_listen_inet->port());
    return true;

  case AF_INET6:
    if (!m_listen_inet6->open(begin, end, m_listen_backlog, bind_address.get()))
      return false;

    config::network_config()->set_listen_port_unsafe(m_listen_inet6->port());
    return true;

  default:
    throw input_error("NetworkManager::open_listen_on_address(): bind address has invalid family.");
  }
}

} // namespace torrent::net
