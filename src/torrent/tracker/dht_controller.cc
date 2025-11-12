#include "config.h"

#include "dht_controller.h"

#include "dht/dht_router.h"
#include "src/manager.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/network_config.h"
#include "torrent/net/network_manager.h"
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
    m_router = std::make_unique<DhtRouter>(dht_cache);

  } catch (const torrent::local_error& e) {
    LT_LOG("initialization failed : %s", e.what());
  }
}

bool
DhtController::start() {
  auto lock = std::lock_guard(m_lock);

  if (m_router == nullptr)
    throw internal_error("DhtController::start() called without initializing first.");

  auto port = config::network_config()->override_dht_port();

  if (port == 0)
    port = runtime::network_manager()->listen_port_or_throw();

  LT_LOG("starting : port:%d", port);

  try {
    m_router->start(port);
    m_port = port;

  } catch (const torrent::local_error& e) {
    LT_LOG("start failed : %s", e.what());
    return false;
  }

  return true;
}

void
DhtController::stop() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    return;

  LT_LOG("stopping", 0);

  m_router->stop();
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

  if (m_router)
    m_router->add_contact(std::move(host), port);
}

void
DhtController::add_node(const sockaddr* sa, int port) {
  auto lock = std::lock_guard(m_lock);

  if (m_router)
    m_router->contact(sa, port);
}

Object*
DhtController::store_cache(Object* container) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::store_cache() called but DHT not initialized.");

  return m_router->store_cache(container);
}

DhtController::statistics_type
DhtController::get_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::get_statistics() called but DHT not initialized.");

  return m_router->get_statistics();
}

void
DhtController::reset_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::reset_statistics() called but DHT not initialized.");

  m_router->reset_statistics();
}

void
DhtController::announce(const HashString& info_hash, TrackerDht* tracker) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::announce() called but DHT not initialized.");

  m_router->announce(info_hash, tracker);
}

void
DhtController::cancel_announce(const HashString* info_hash, const torrent::TrackerDht* tracker) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::cancel_announce() called but DHT not initialized.");

  m_router->cancel_announce(info_hash, tracker);
}

} // namespace torrent::tracker
