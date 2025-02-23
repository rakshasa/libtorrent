#include "config.h"

#include "torrent/tracker/tracker_wrappers.h"

#include "torrent/tracker_controller.h"
#include "torrent/utils/log.h"

// TODO: Remove old logging categories.

// #define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
//   lt_log_print(LOG_TRACKER_EVENTS, "tracker_manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

bool
TrackerControllerWrapper::is_active() const {
  return m_ptr->is_active();
}

bool
TrackerControllerWrapper::is_requesting() const {
  return m_ptr->is_requesting();
}

bool
TrackerControllerWrapper::is_failure_mode() const {
  return m_ptr->is_failure_mode();
}

bool
TrackerControllerWrapper::is_promiscuous_mode() const {
  return m_ptr->is_promiscuous_mode();
}

uint32_t
TrackerControllerWrapper::seconds_to_next_timeout() const {
  return m_ptr->seconds_to_next_timeout();
}

uint32_t
TrackerControllerWrapper::seconds_to_next_scrape() const {
  return m_ptr->seconds_to_next_scrape();
}

void
TrackerControllerWrapper::enable() {
  m_ptr->enable();
}

void
TrackerControllerWrapper::enable_dont_reset_stats() {
  m_ptr->enable(TrackerController::enable_dont_reset_stats);
}

void
TrackerControllerWrapper::disable() {
  m_ptr->disable();
}

void
TrackerControllerWrapper::close() {
  m_ptr->close();
}

void
TrackerControllerWrapper::send_start_event() {
  m_ptr->send_start_event();
}

void
TrackerControllerWrapper::send_stop_event() {
  m_ptr->send_stop_event();
}

void
TrackerControllerWrapper::send_completed_event() {
  m_ptr->send_completed_event();
}

void
TrackerControllerWrapper::send_update_event() {
  m_ptr->send_update_event();
}

void
TrackerControllerWrapper::start_requesting() {
  m_ptr->start_requesting();
}

void
TrackerControllerWrapper::stop_requesting() {
  m_ptr->stop_requesting();
}

void
TrackerControllerWrapper::manual_request(bool request_now) {
  m_ptr->manual_request(request_now);
}

void
TrackerControllerWrapper::scrape_request(uint32_t seconds_to_request) {
  m_ptr->scrape_request(seconds_to_request);
}

void
TrackerControllerWrapper::set_slots(slot_address_list success, slot_string failure) {
  m_ptr->slot_success() = success;
  m_ptr->slot_failure() = failure;
}

} // namespace torrent
