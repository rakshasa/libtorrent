#include "config.h"

#include "dht_controller.h"

#include "dht/dht_router.h"
#include "src/manager.h"
#include "torrent/exceptions.h"
#include "torrent/throttle.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/network_config.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(torrent::LOG_DHT, "dht_controller", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

DhtController::DhtController() = default;

DhtController::~DhtController() {
  auto lock = std::lock_guard(m_lock);
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
  auto bind_address = config::network_config()->bind_address();

  if (bind_address->sa_family == AF_UNSPEC)
    bind_address = sa_make_inet6_any();

  LT_LOG("initializing : %s", sa_pretty_str(bind_address.get()).c_str());

  if (m_router != NULL)
    throw internal_error("DhtController::initialize called with DHT already active.");

  try {
    m_router = std::make_unique<DhtRouter>(dht_cache, bind_address.get());
  } catch (const torrent::local_error& e) {
    LT_LOG("initialization failed : %s", e.what());
  }
}

bool
DhtController::start(uint16_t port) {
  auto lock = std::lock_guard(m_lock);

  LT_LOG("starting : port:%d", port);

  if (m_router == nullptr)
    throw internal_error("DhtController::start called without initializing first.");

  m_port = port;

  try {
    m_router->start(port);
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
}

void
DhtController::set_receive_requests(bool state) {
  auto lock = std::lock_guard(m_lock);
  m_receive_requests = state;
}

void
DhtController::add_node(const sockaddr* sa, int port) {
  auto lock = std::lock_guard(m_lock);

  if (m_router)
    m_router->contact(sa, port);
}

void
DhtController::add_node(const std::string& host, int port) {
  auto lock = std::lock_guard(m_lock);

  if (m_router)
    m_router->add_contact(host, port);
}

Object*
DhtController::store_cache(Object* container) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::store_cache called but DHT not initialized.");

  return m_router->store_cache(container);
}

DhtController::statistics_type
DhtController::get_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::get_statistics called but DHT not initialized.");

  return m_router->get_statistics();
}

void
DhtController::reset_statistics() {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::reset_statistics called but DHT not initialized.");

  m_router->reset_statistics();
}

// TOOD: Throttle needs to be made thread-safe.

void
DhtController::set_upload_throttle(Throttle* t) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::set_upload_throttle() called but DHT not initialized.");

  if (m_router->is_active())
    throw internal_error("DhtController::set_upload_throttle() called while DHT server active.");

  m_router->set_upload_throttle(t->throttle_list());
}

void
DhtController::set_download_throttle(Throttle* t) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::set_download_throttle() called but DHT not initialized.");

  if (m_router->is_active())
    throw internal_error("DhtController::set_download_throttle() called while DHT server active.");

  m_router->set_download_throttle(t->throttle_list());
}

void
DhtController::announce(const HashString& info_hash, TrackerDht* tracker) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::announce called but DHT not initialized.");

  m_router->announce(info_hash, tracker);
}

void
DhtController::cancel_announce(const HashString* info_hash, const torrent::TrackerDht* tracker) {
  auto lock = std::lock_guard(m_lock);

  if (!m_router)
    throw internal_error("DhtController::cancel_announce called but DHT not initialized.");

  m_router->cancel_announce(info_hash, tracker);
}

} // namespace torrent::tracker
