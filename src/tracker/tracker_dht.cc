#include "config.h"

#include "tracker/tracker_dht.h"

#include <cassert>

#include "dht/dht_router.h"
#include "manager.h"
#include "torrent/exceptions.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/system/callbacks.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_dht", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerDht::TrackerDht(const TrackerInfo& info, int flags)
  : TrackerWorker(info, flags) {

  if (!runtime::network_manager()->dht_controller()->is_valid())
    throw internal_error("Trying to add DHT tracker with no DHT manager.");

  m_delay_clear_state.slot() = [this] {
    m_dht_state = state_idle;
    update_requesting_state();
  };
}

tracker_enum
TrackerDht::type() const {
  return TRACKER_DHT;
}

void
TrackerDht::send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) {
  assert(!m_weak_tracker.expired());

  LT_LOG("sending event : state:%s dht_state:%s replied:%d contacted:%d",
         option_as_string(OPTION_TRACKER_EVENT, new_state), states[m_dht_state], m_replied.load(), m_contacted.load());

  close();

  lock_and_set_latest_event(new_state);

  if (new_state == tracker::TrackerState::EVENT_STOPPED)
    return;

  if (!runtime::network_manager()->is_dht_active())
    return receive_failed("DHT is not enabled.");

  m_params    = params;
  m_dht_state = state_searching;

  update_requesting_state();

  runtime::network_manager()->dht_controller()->announce(info().info_hash, m_weak_tracker);

  state().set_normal_interval(20 * 60s);
  state().set_min_interval(0s);
}

void
TrackerDht::send_scrape([[maybe_unused]] tracker::TrackerParams params) {
  throw internal_error("Tracker type DHT does not support scrape.");
}

void
TrackerDht::close() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());
  assert(!m_weak_tracker.expired());

  LT_LOG("closing event : dht_state:%s replied:%d contacted:%d",
         states[m_dht_state], m_replied.load(), m_contacted.load());

  this_thread::scheduler()->erase(&m_delay_clear_state);

  runtime::network_manager()->dht_controller()->cancel_announce(info().info_hash, m_weak_tracker);

  // TODO: Moved check from send_event(), verify if this is correct.
  // if (m_dht_state != state_idle)
  //   throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");

  remove_events();
  update_requesting_state();
}

void
TrackerDht::cleanup() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());
  assert(!m_weak_tracker.expired());

  this_thread::scheduler()->erase(&m_delay_clear_state);

  // This still queues a cancel request, however 'this' is never accessed.
  //
  // Even if we accidentally create a new TrackerDht with the same address, the cancel request will
  // not do anything harmful.
  runtime::network_manager()->dht_controller()->cancel_announce(info().info_hash, m_weak_tracker);

  auto guard = lock_guard();
  state().m_flags |=  tracker::TrackerState::flag_deleted;
  state().m_flags &= ~tracker::TrackerState::flag_requesting;
  state().m_flags &= ~tracker::TrackerState::flag_starting_request;
}

// TODO: We don't really need to track announcing state in Tracker?
void
TrackerDht::set_dht_announce_state() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());

  this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_clear_state, 2min);

  m_dht_state = state_announcing;

  update_requesting_state();
}

void
TrackerDht::receive_peers(AddressList&& address_list) {
  LT_LOG("received peers : dht_state:%s replied:%d contacted:%d size:%" PRIu32,
         states[m_dht_state], m_replied.load(), m_contacted.load(), address_list.size());

  m_slot_new_peers(std::move(address_list));
}

void
TrackerDht::receive_success() {
  LT_LOG("received success : dht_state:%s replied:%d contacted:%d",
         states[m_dht_state], m_replied.load(), m_contacted.load());

  m_dht_state = state_idle;
  update_requesting_state();

  m_slot_success({});
}

void
TrackerDht::receive_failed(const char* msg) {
  LT_LOG("received failure : dht_state:%s replied:%d contacted:%d msg:%s",
         states[m_dht_state], m_replied.load(), m_contacted.load(), msg);

  m_dht_state = state_idle;

  update_requesting_state();

  m_slot_failure(msg);
}

void
TrackerDht::receive_progress(int replied, int contacted) {
  LT_LOG("received progress : dht_state:%s replied:%d contacted:%d",
         states[m_dht_state], replied, contacted);

  m_replied   = replied;
  m_contacted = contacted;
}

void
TrackerDht::add_event(std::weak_ptr<TrackerDht> weak_tracker, std::function<void (TrackerDht*)>&& event) {
  auto tracker = weak_tracker.lock();

  if (tracker == nullptr)
    return;

  tracker_thread::callback(tracker->callback_id(), [weak_tracker, event = std::move(event)]() mutable {
      auto tracker = weak_tracker.lock();

      if (tracker == nullptr)
        return;

      event(tracker.get());
    });
}

void
TrackerDht::update_requesting_state() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());

  auto guard = lock_guard();

  if (m_dht_state != state_announcing)
    this_thread::scheduler()->erase(&m_delay_clear_state);

  state().m_flags &= ~tracker::TrackerState::flag_starting_request;

  if (m_dht_state != state_idle)
    state().m_flags |= tracker::TrackerState::flag_requesting;
  else
    state().m_flags &= ~tracker::TrackerState::flag_requesting;
}

} // namespace torrent
