#include "config.h"

#include "torrent/runtime/network_manager.h"

#include <cassert>

#include "net/listen.h"
#include "torrent/exceptions.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/runtime.h"
#include "torrent/system/thread.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/utils/log.h"

// TODO: Add runtime category and add it to important/complete log outputs.

#define LT_LOG_NOTICE(log_fmt, ...)                                     \
  lt_log_print_subsystem(LOG_NOTICE, "runtime::network_manager", log_fmt, __VA_ARGS__);

namespace torrent::runtime {

NetworkManager::NetworkManager()
  : m_listen_inet(new Listen),
    m_listen_inet6(new Listen),
    m_dht_controller(new tracker::DhtController) {
}

NetworkManager::~NetworkManager() = default;

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
}

void
NetworkManager::listen_restart() {
  auto guard = lock_guard();
  listen_restart_unsafe();
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

void
NetworkManager::set_listen_port(uint16_t port) {
  {
    auto guard = lock_guard();

    m_listen_port = port;

    listen_restart_unsafe();
  }

  // TODO: Replace with a set_dht_port function.
  // if (!is_network_initialized() || network_config()->is_block_incoming())
  //   return;

  // if (runtime::network_config()->override_dht_port() != 0)
  //   return;

  // if (!runtime::network_manager()->dht_controller()->is_active())
  //   return;

  // try {
  //   runtime::network_manager()->dht_controller()->stop();
  //   runtime::network_manager()->dht_controller()->start();

  // } catch (const base_error& e) {
  //   LT_LOG_NOTICE("Could not restart DHT server: %" PRIu16 " : %s", e.what());
  //   return;
  // }
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

bool
NetworkManager::listen_open_unsafe(uint16_t first, uint16_t last) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  if (m_listen_inet->is_open() || m_listen_inet6->is_open())
    throw internal_error("NetworkManager::open_listen(): Tried to open listen socket when one is already open.");

  Listen::open_options options = {
    .first_port = first,
    .last_port  = last,
  };

  NetworkConfig::listen_addresses listen_addresses;

  {
    auto nw_config    = runtime::network_config();
    auto config_guard = nw_config->lock_guard();

    options.backlog  = nw_config->listen_backlog_unsafe();
    listen_addresses = nw_config->listen_addresses_unsafe();

    if (first != last && nw_config->override_dht_port_unsafe() == 0)
      options.check_dht = true;
  }

  auto [inet_address, inet6_address, block_ipv4in6] = listen_addresses;

  options.block_ipv4in6 = block_ipv4in6;

  if (inet_address == nullptr && inet6_address == nullptr)
    throw input_error("Neither IPv4 nor IPv6 listen address are suitable for opening listen sockets, check block_ipv4 and block_ipv6 settings.");

  if (inet_address != nullptr && inet6_address != nullptr) {
    if (!Listen::open_both(m_listen_inet.get(), m_listen_inet6.get(), inet_address.get(), inet6_address.get(), options))
      return false;

    m_listen_port = m_listen_inet->port();
    return true;
  }

  if (inet_address != nullptr) {
    if (!Listen::open_single(m_listen_inet.get(), inet_address.get(), options))
      return false;

    m_listen_port = m_listen_inet->port();
    return true;
  }

  if (inet6_address != nullptr) {
    if (!Listen::open_single(m_listen_inet6.get(), inet6_address.get(), options))
      return false;

    m_listen_port = m_listen_inet6->port();
    return true;
  }

  throw internal_error("NetworkManager::open_listen(): reached unreachable code.");
}

void
NetworkManager::listen_close_unsafe() {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  m_listen_inet->close();
  m_listen_inet6->close();
}

void
NetworkManager::listen_restart_unsafe() {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  listen_close_unsafe();

  // TODO: If listen port was always 0, we need to properly open it using port range/randomizing.
  if (m_listen_port == 0)
    return;

  if (!is_network_initialized() || network_config()->is_block_incoming())
    return;

  try {
    listen_open_unsafe(m_listen_port, m_listen_port);

  } catch (const base_error& e) {
    LT_LOG_NOTICE("Could not restart listen socket: %" PRIu16 " : %s", e.what());
    return;
  }
}

} // namespace torrent::runtime
