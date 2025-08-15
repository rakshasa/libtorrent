#include "config.h"

#include "dht_controller.h"

#include "dht/dht_router.h"
#include "src/manager.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/throttle.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

// TODO: Rename to DHT_CONTROLLER

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_subsystem(torrent::LOG_DHT_MANAGER, "dht_manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

DhtController::DhtController() = default;

DhtController::~DhtController() {
  stop();
}

bool
DhtController::is_valid() {
  auto lock = std::scoped_lock(m_lock);

  return m_router != nullptr;
}

bool
DhtController::is_active() {
  auto lock = std::scoped_lock(m_lock);

  return m_router && m_router->is_active();
}

bool
DhtController::is_receiving_requests() {
  auto lock = std::scoped_lock(m_lock);

  return m_receive_requests;
}

uint16_t
DhtController::port() {
  auto lock = std::scoped_lock(m_lock);

  return m_port;
}

void
DhtController::initialize(const Object& dhtCache) {
  auto bind_address = manager->connection_manager()->bind_address();

  sa_unique_ptr tmp_sa;

  if (bind_address->sa_family == AF_UNSPEC) {
    tmp_sa = sa_make_inet6_any();
    bind_address = tmp_sa.get();
  }

  LT_LOG_THIS("initializing : %s", sa_pretty_str(bind_address).c_str());

  if (m_router != NULL)
    throw internal_error("DhtController::initialize called with DHT already active.");

  try {
    m_router = std::make_unique<DhtRouter>(dhtCache, bind_address);

  } catch (const torrent::local_error& e) {
    LT_LOG_THIS("initialization failed : %s", e.what());
  }
}

bool
DhtController::start(ConnectionManager::port_type port) {
  LT_LOG_THIS("starting : port:%d", port);

  if (m_router == nullptr)
    throw internal_error("DhtController::start called without initializing first.");

  m_port = port;

  try {
    m_router->start(port);
    return true;

  } catch (const torrent::local_error& e) {
    LT_LOG_THIS("start failed : %s", e.what());
    return false;
  }
}

void
DhtController::stop() {
  if (!m_router)
    return;

  LT_LOG_THIS("stopping", 0);
  m_router->stop();
}

void
DhtController::set_receive_requests(bool state) {
  auto lock = std::scoped_lock(m_lock);

  m_receive_requests = state;
}

void
DhtController::add_node(const sockaddr* sa, int port) {
  if (m_router)
    m_router->contact(sa, port);
}

void
DhtController::add_node(const std::string& host, int port) {
  if (m_router)
    m_router->add_contact(host, port);
}

Object*
DhtController::store_cache(Object* container) {
  if (!m_router)
    throw internal_error("DhtController::store_cache called but DHT not initialized.");

  return m_router->store_cache(container);
}

DhtController::statistics_type
DhtController::get_statistics() const {
  if (!m_router)
    throw internal_error("DhtController::get_statistics called but DHT not initialized.");

  return m_router->get_statistics();
}

void
DhtController::reset_statistics() {
  if (!m_router)
    throw internal_error("DhtController::reset_statistics called but DHT not initialized.");

  m_router->reset_statistics();
}

// TOOD: Throttle needs to be made thread-safe.

void
DhtController::set_upload_throttle(Throttle* t) {
  if (!m_router)
    throw internal_error("DhtController::set_upload_throttle() called but DHT not initialized.");

  if (m_router->is_active())
    throw internal_error("DhtController::set_upload_throttle() called while DHT server active.");

  m_router->set_upload_throttle(t->throttle_list());
}

void
DhtController::set_download_throttle(Throttle* t) {
  if (!m_router)
    throw internal_error("DhtController::set_download_throttle() called but DHT not initialized.");

  if (m_router->is_active())
    throw internal_error("DhtController::set_download_throttle() called while DHT server active.");

  m_router->set_download_throttle(t->throttle_list());
}

void
DhtController::announce(const HashString& info_hash, TrackerDht* tracker) {
  if (!m_router)
    throw internal_error("DhtController::announce called but DHT not initialized.");

  m_router->announce(info_hash, tracker);
}

void
DhtController::cancel_announce(const HashString* info_hash, const torrent::TrackerDht* tracker) {
  if (!m_router)
    throw internal_error("DhtController::cancel_announce called but DHT not initialized.");

  m_router->cancel_announce(info_hash, tracker);
}

} // namespace torrent::tracker
