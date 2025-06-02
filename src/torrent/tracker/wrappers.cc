#include "config.h"

#include "torrent/tracker/wrappers.h"

#include <utility>

#include "tracker/tracker_list.h"
#include "tracker/tracker_controller.h"

namespace torrent::tracker {

// Must be called from main thread.

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

bool
TrackerControllerWrapper::has_active_trackers() const {
  return m_ptr->tracker_list()->has_active();
}

bool
TrackerControllerWrapper::has_active_trackers_not_scrape() const {
  return m_ptr->tracker_list()->has_active_not_scrape();
}

bool
TrackerControllerWrapper::has_usable_trackers() const {
  return m_ptr->tracker_list()->has_usable();
}

uint32_t
TrackerControllerWrapper::key() const {
  return m_ptr->tracker_list()->key();
}

int32_t
TrackerControllerWrapper::numwant() const {
  return m_ptr->tracker_list()->numwant();
}

void
TrackerControllerWrapper::set_numwant(int32_t n) {
  m_ptr->tracker_list()->set_numwant(n);
}

uint32_t
TrackerControllerWrapper::size() const {
  return m_ptr->tracker_list()->size();
}

Tracker
TrackerControllerWrapper::at(uint32_t index) const {
  // TODO: This function should make it a hard exception if the index is out of range, and have a
  // different function that can return invalid trackers.

  return m_ptr->tracker_list()->at(index);
}

void
TrackerControllerWrapper::add_extra_tracker(uint32_t group, const std::string& url) {
  m_ptr->tracker_list()->insert_url(group, url, true);
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
TrackerControllerWrapper::manual_request(bool request_now) {
  m_ptr->manual_request(request_now);
}

void
TrackerControllerWrapper::scrape_request(uint32_t seconds_to_request) {
  m_ptr->scrape_request(seconds_to_request);
}

void
TrackerControllerWrapper::cycle_group(uint32_t group) {
  m_ptr->tracker_list()->cycle_group(group);
}

// Algorithms:

Tracker
TrackerControllerWrapper::find_if(const std::function<bool(Tracker&)>& f) {
  for (auto& tracker : *m_ptr->tracker_list()) {
    if (f(tracker))
      return tracker;
  }

  return Tracker(std::shared_ptr<torrent::TrackerWorker>());
}

void
TrackerControllerWrapper::for_each(const std::function<void(Tracker&)>& f) {
  for (auto& tracker : *m_ptr->tracker_list())
    f(tracker);
}

Tracker
TrackerControllerWrapper::c_find_if(const std::function<bool(const Tracker&)>& f) const {
  for (auto& tracker : *m_ptr->tracker_list()) {
    if (f(tracker))
      return tracker;
  }

  return Tracker(std::shared_ptr<torrent::TrackerWorker>());
}

void
TrackerControllerWrapper::c_for_each(const std::function<void(const Tracker&)>& f) const {
  for (auto& tracker : *m_ptr->tracker_list())
    f(tracker);
}

// Private:

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
TrackerControllerWrapper::set_slots(slot_address_list success, slot_string failure) {
  m_ptr->slot_success() = std::move(success);
  m_ptr->slot_failure() = std::move(failure);
}

} // namespace torrent::tracker
