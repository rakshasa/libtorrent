#include "config.h"

#include "torrent/net/network_manager.h"

#include "net/listen.h"
#include "torrent/exceptions.h"
#include "torrent/net/network_config.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/utils/log.h"

// TODO: Add net category and add it to important/complete log outputs.

#define LT_LOG_NOTICE(log_fmt, ...)                                     \
  lt_log_print_subsystem(LOG_NOTICE, "net::network_manager", log_fmt, __VA_ARGS__);

namespace torrent::net {

NetworkManager::NetworkManager()
  : m_listen_inet(new Listen),
    m_listen_inet6(new Listen),
    m_dht_controller(new tracker::DhtController) {
}

NetworkManager::~NetworkManager() {
  main_thread::cancel_callback_and_wait(this);
}

bool
NetworkManager::is_listening() const {
  auto guard = lock_guard();
  return is_listening_unsafe();
}

bool
NetworkManager::is_dht_valid() const {
  // auto guard = lock_guard();
  return m_dht_controller->is_valid();
}

bool
NetworkManager::is_dht_active() const {
  // auto guard = lock_guard();
  return m_dht_controller->is_active();
}

bool
NetworkManager::is_dht_active_and_receiving_requests() const {
  // auto guard = lock_guard();
  return m_dht_controller->is_active() && m_dht_controller->is_receiving_requests();
}

// Cleanup should get called twice; when shutdown is initiated and on libtorrent cleanup.
void
NetworkManager::cleanup() {
  auto guard = lock_guard();

  m_dht_controller->stop();
  listen_close_unsafe();
}

// TODO: Currently only opens one listen socket, either ipv4 or ipv6 based on bind address.
// TODO: Log here
// TODO: Verify various combinations of addresses/block_ipv4in6 match tcp46/udp46 sockets

// TODO: Fix DHT

bool
NetworkManager::listen_open(uint16_t begin, uint16_t end) {
  auto guard = lock_guard();
  return listen_open_unsafe(begin, end);
}

void
NetworkManager::listen_close() {
  auto guard = lock_guard();
  listen_close_unsafe();

  // m_dht_controller->stop();
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

uint16_t
NetworkManager::dht_port() {
  // auto guard = lock_guard();
  return m_dht_controller->port();
}

// TODO: Make bootstrap nodes explicit, and when we add them also try adding as node if we're
// already running.

// TODO: Consider adding dht bootstrap nodes to network config.

void
NetworkManager::dht_add_bootstrap_node(std::string host, int port){
  // auto guard = lock_guard();
  m_dht_controller->add_bootstrap_node(std::move(host), port);
}

void
NetworkManager::dht_add_peer_node([[maybe_unused]] const sockaddr* sa, [[maybe_unused]] int port) {
  // Ignore peer nodes as we shouldn't need them(?)
  //
  // Re-enable this if it causes issues.
}

bool
NetworkManager::is_listening_unsafe() const {
  return m_listen_inet->is_open() || m_listen_inet6->is_open();
}

void
NetworkManager::restart_listen() {
  auto guard = lock_guard();

  if (m_listen_restarting || !is_listening_unsafe())
    return;

  m_listen_restarting = true;

  main_thread::callback(this, [this]() { perform_restart_listen(); });
}

bool
NetworkManager::listen_open_unsafe(uint16_t begin, uint16_t end) {
  auto config_guard = config::network_config()->lock_guard();
  auto backlog      = config::network_config()->listen_backlog_unsafe();

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
NetworkManager::listen_close_unsafe() {
  m_listen_inet->close();
  m_listen_inet6->close();
}

void
NetworkManager::perform_restart_listen() {
  auto guard = lock_guard();

  m_listen_restarting = false;

  if (!is_listening_unsafe())
    return;

  // TODO: Move DHT here and restart.

  listen_close_unsafe();

  try {
    listen_open_unsafe(m_listen_port, m_listen_port);
  } catch (const base_error& e) {
    LT_LOG_NOTICE("could not restart listen socket: %s", e.what());
  }
}

} // namespace torrent::net
