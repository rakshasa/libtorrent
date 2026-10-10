#include "config.h"

#include "dht_controller.h"

#include "dht/dht_router.h"
#include "src/manager.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/system/callbacks.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(torrent::LOG_DHT_CONTROLLER, "dht_controller", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

DhtController::DhtController() = default;

DhtController::~DhtController() {
  stop();
}

bool
DhtController::is_valid() {
  auto lock = std::lock_guard(m_lock);
  return m_router != nullptr;
}

bool
DhtController::is_active() {
  auto lock = std::lock_guard(m_lock);
  return m_router && m_router->is_active();
}

bool
DhtController::is_receiving_requests() {
  auto lock = std::lock_guard(m_lock);
  return m_receive_requests;
}

uint16_t
DhtController::port() {
  auto lock = std::lock_guard(m_lock);
  return m_port;
}

void
DhtController::initialize(const Object& dht_cache) {
  auto lock = std::lock_guard(m_lock);

  if (m_router != nullptr)
    throw internal_error("DhtController::initialize() called with DHT already active.");

  LT_LOG("initializing", 0);

  try {
    m_router = std::make_unique<DhtRouter>(this, dht_cache, AF_INET);

  } catch (const torrent::local_error& e) {
    LT_LOG("initialization failed : %s", e.what());
    return;
  }

  // The IPv6 DHT (BEP 32) shares the node ID; a cache it cannot read costs it its nodes, never the IPv4 DHT.
  try {
    m_router6 = std::make_unique<DhtRouter>(this, dht_cache, AF_INET6);

  } catch (const torrent::local_error& e) {
    LT_LOG("initialization of the inet6 router failed, retrying without its cached nodes : %s", e.what());

    try {
      auto cache = Object::create_map();

      if (dht_cache.is_map() && dht_cache.has_key_string("self_id"))
        cache.insert_key("self_id", dht_cache.get_key_string("self_id"));

      m_router6 = std::make_unique<DhtRouter>(this, cache, AF_INET6);

    } catch (const torrent::local_error& e2) {
      LT_LOG("initialization of the inet6 router failed : %s", e2.what());
    }
  }
}

bool
DhtController::start() {
  auto lock = std::lock_guard(m_lock);

  if (m_router == nullptr)
    throw internal_error("DhtController::start() called without initializing first.");

  auto port = runtime::network_config()->override_dht_port();

  if (port == 0)
    port = runtime::network_manager()->listen_port_or_throw();

  LT_LOG("starting : port:%d", port);

  bool started = false;

  try {
    m_router->start(port);
    started = true;

  } catch (const torrent::local_error& e) {
    LT_LOG("start failed : %s", e.what());
  }

  // IPv6 only when the network config offers an inet6 UDP address (network.block.ipv6 and UDP blocking say no).
  if (m_router6 != nullptr && std::get<2>(runtime::network_config()->bind_udp_addresses_or_null()) != nullptr) {
    try {
      m_router6->start(port);
      started = true;

      LT_LOG("started inet6 : port:%d", port);

    } catch (const torrent::local_error& e) {
      LT_LOG("inet6 start failed : %s", e.what());
    }
  }

  if (started)
    m_port = port;

  return started;
}

void
DhtController::stop() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    return;

  LT_LOG("stopping", 0);

  m_router->stop();

  if (m_router6)
    m_router6->stop();

  m_port = 0;
}

void
DhtController::set_receive_requests(bool state) {
  auto lock = std::lock_guard(m_lock);
  m_receive_requests = state;
}

void
DhtController::add_bootstrap_node(std::string host, int port) {
  auto lock = std::lock_guard(m_lock);

  // Each router resolves the host in its own family: a host without an AAAA record is simply no IPv6 contact.
  if (m_router6)
    m_router6->add_bootstrap_contact(host, port);

  if (m_router)
    m_router->add_bootstrap_contact(std::move(host), port);
}

// The router of the address's family, a v4-mapped address counting as IPv4.
DhtRouter*
DhtController::router_for_unsafe(const sockaddr* sa) {
  if (sa_copy_unmapped(sa)->sa_family == AF_INET6)
    return m_router6.get();

  return m_router.get();
}

void
DhtController::add_node(const sockaddr* sa, int port) {
  auto lock = std::lock_guard(m_lock);

  if (auto router = router_for_unsafe(sa))
    router->contact(sa, port);
}

void
DhtController::add_peer_node(const sockaddr* sa, int port) {
  auto lock = std::lock_guard(m_lock);

  auto router = router_for_unsafe(sa);

  if (router == nullptr)
    return;

  auto& populated = router->family() == AF_INET6 ? m_nodes_populated6 : m_nodes_populated;

  if (populated.load(std::memory_order_relaxed))
    return;

  router->contact(sa, port);
}

void
DhtController::set_nodes_populated(int family, bool state) {
  (family == AF_INET6 ? m_nodes_populated6 : m_nodes_populated).store(state, std::memory_order_relaxed);
}

Object*
DhtController::store_cache(Object* container) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::store_cache() called but DHT not initialized.");

  m_router->store_cache(container);

  if (m_router6)
    m_router6->store_cache(container);

  return container;
}

DhtController::statistics_type
DhtController::get_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::get_statistics() called but DHT not initialized.");

  bool active4 = m_router->is_active();
  bool active6 = m_router6 && m_router6->is_active();

  // The main fields stay the IPv4 DHT's (rtorrent's firewall checks read them) unless only IPv6 runs.
  auto stats = (!active4 && active6) ? m_router6->get_statistics() : m_router->get_statistics();

  if (m_router6) {
    auto stats6 = m_router6->get_statistics();

    stats.active6           = active6;
    stats.cycle6            = stats6.cycle;
    stats.queries_received6 = stats6.queries_received;
    stats.queries_sent6     = stats6.queries_sent;
    stats.replies_received6 = stats6.replies_received;
    stats.num_nodes6        = stats6.num_nodes;
    stats.num_buckets6      = stats6.num_buckets;
    stats.num_peers6        = stats6.num_peers;
    stats.num_trackers6     = stats6.num_trackers;
  }

  return stats;
}

void
DhtController::reset_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::reset_statistics() called but DHT not initialized.");

  m_router->reset_statistics();

  if (m_router6)
    m_router6->reset_statistics();
}

// We don't care about the tracker or download being deleted as that's a rare edge-case that's
// unnessesary to optimize for.
//
// Instead we depend on the callbacks from DHT to check if weak_ptr is expired.

void
DhtController::announce(const HashString& info_hash, std::weak_ptr<TrackerDht> weak_tracker) {
  main_thread::callback([this, info_hash, weak_tracker] {
      auto lock = std::lock_guard(m_lock);

      if (!m_router)
        throw internal_error("DhtController::announce() called but DHT not initialized.");

      // Both families search. The IPv4 DHT reports to the tracker while it runs; the IPv6 DHT delivers its peers,
      // and reports only when it is the one running.
      bool active4 = m_router->is_active();
      bool active6 = m_router6 && m_router6->is_active();

      if (active4 || !active6)
        m_router->announce(info_hash, weak_tracker, true);

      if (active6)
        m_router6->announce(info_hash, weak_tracker, !active4);
    });
}

void
DhtController::cancel_announce(const HashString& info_hash, std::weak_ptr<TrackerDht> weak_tracker) {
  main_thread::callback([this, info_hash, weak_tracker] {
      auto lock = std::lock_guard(m_lock);

      if (!m_router)
        throw internal_error("DhtController::cancel_announce() called but DHT not initialized.");

      m_router->cancel_announce(info_hash, weak_tracker);

      if (m_router6)
        m_router6->cancel_announce(info_hash, weak_tracker);
    });
}

} // namespace torrent::tracker
