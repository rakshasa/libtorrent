#include "config.h"

#include "tracker/tracker_dht.h"

#include "dht/dht_router.h"
#include "manager.h"
#include "torrent/exceptions.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/tracker/dht_controller.h"
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

TrackerDht::~TrackerDht() {
  this_thread::scheduler()->erase(&m_delay_clear_state);

  runtime::network_manager()->dht_controller()->cancel_announce(NULL, this);
}

tracker_enum
TrackerDht::type() const {
  return TRACKER_DHT;
}

void
TrackerDht::send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) {
  LT_LOG("sending event : state:%s dht_state:%s replied:%d contacted:%d",
         option_as_string(OPTION_TRACKER_EVENT, new_state), states[m_dht_state], m_replied.load(), m_contacted.load());

  close();

  lock_and_set_latest_event(new_state);

  if (new_state == tracker::TrackerState::EVENT_STOPPED)
    return;

  if (!runtime::network_manager()->dht_controller()->is_active())
    return receive_failed("DHT is not enabled.");

  m_params    = params;
  m_dht_state = state_searching;

  update_requesting_state();

  runtime::network_manager()->dht_controller()->announce(info().info_hash, this);

  state().set_normal_interval(20 * 60);
  state().set_min_interval(0);
}

void
TrackerDht::send_scrape(tracker::TrackerParams params) {
  throw internal_error("Tracker type DHT does not support scrape.");
}

void
TrackerDht::close() {
  LT_LOG("closing event : dht_state:%s replied:%d contacted:%d",
         states[m_dht_state], m_replied.load(), m_contacted.load());

  runtime::network_manager()->dht_controller()->cancel_announce(&info().info_hash, this);

  // TODO: Moved check from send_event(), verify if this is correct.
  // if (m_dht_state != state_idle)
  //   throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");

  update_requesting_state();

  m_slot_close();
}

// TODO: We don't really need to track announcing state in Tracker?
void
TrackerDht::set_dht_announce_state() {
  this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_clear_state, 2min);

  m_dht_state = state_announcing;

  update_requesting_state();
}

void
TrackerDht::receive_peers(raw_list peers) {
  LT_LOG("received peers : dht_state:%s replied:%d contacted:%d raw_peers_size:%" PRIu32,
         states[m_dht_state], m_replied.load(), m_contacted.load(), peers.size());

  // if (m_dht_state == state_idle)
  //   throw internal_error("TrackerDht::receive_peers called while not busy.");

  // The final success event will resend all peers for now.
  m_peers.parse_address_bencode(peers);

  AddressList address_list;
  address_list.parse_address_bencode(peers);

  m_slot_new_peers(std::move(address_list));
}

void
TrackerDht::receive_success() {
  LT_LOG("received success : dht_state:%s replied:%d contacted:%d peers:%zu",
         states[m_dht_state], m_replied.load(), m_contacted.load(), m_peers.size());

  // if (m_dht_state == state_idle)
  //   throw internal_error("TrackerDht::receive_success called while not busy.");

  m_dht_state = state_idle;
  update_requesting_state();

  m_slot_success(std::move(m_peers));
}

void
TrackerDht::receive_failed(const char* msg) {
  LT_LOG("received failure : dht_state:%s replied:%d contacted:%d msg:%s",
         states[m_dht_state], m_replied.load(), m_contacted.load(), msg);

  // if (m_dht_state == state_idle)
  //   throw internal_error("TrackerDht::receive_failed called while not busy.");

  m_dht_state = state_idle;
  update_requesting_state();

  m_slot_failure(msg);
  m_peers.clear();
}

void
TrackerDht::receive_progress(int replied, int contacted) {
  LT_LOG("received progress : dht_state:%s replied:%d contacted:%d",
         states[m_dht_state], replied, contacted);

  // if (m_dht_state == state_idle)
  //   throw internal_error("TrackerDht::receive_status called while not busy.");

  m_replied   = replied;
  m_contacted = contacted;
}

void
TrackerDht::update_requesting_state() {
  auto guard = lock_guard();

  if (m_dht_state != state_announcing)
    this_thread::scheduler()->erase(&m_delay_clear_state);

  if (m_dht_state != state_idle)
    state().m_flags |= tracker::TrackerState::flag_requesting;
  else
    state().m_flags &= ~tracker::TrackerState::flag_requesting;
}

} // namespace torrent
