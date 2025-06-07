#include "config.h"

#include "tracker/tracker_dht.h"

#include "dht/dht_router.h"
#include "manager.h"
#include "torrent/exceptions.h"
#include "torrent/tracker/dht_controller.h"

namespace torrent {

bool TrackerDht::is_allowed() { return manager->dht_controller()->is_valid(); }

TrackerDht::TrackerDht(const TrackerInfo& info, int flags) :
  TrackerWorker(info, flags)
  {

  if (!manager->dht_controller()->is_valid())
    throw internal_error("Trying to add DHT tracker with no DHT manager.");
}

TrackerDht::~TrackerDht() {
  if (m_dht_state != state_idle)
    manager->dht_controller()->cancel_announce(NULL, this);
}

bool
TrackerDht::is_busy() const {
  return m_dht_state != state_idle;
}

bool
TrackerDht::is_usable() const {
  return state().is_enabled() && manager->dht_controller()->is_active();
}


std::string
TrackerDht::lock_and_status() const {
  auto guard = lock_guard();

  if (m_dht_state == state_idle)
    return "[idle]";

  return "[" + std::string(states[m_dht_state]) + ": " + std::to_string(m_replied) + "/" + std::to_string(m_contacted) + " nodes replied]";
}

void
TrackerDht::send_event(tracker::TrackerState::event_enum new_state) {
  close();

  if (m_dht_state != state_idle) {
    manager->dht_controller()->cancel_announce(&info().info_hash, this);

    if (m_dht_state != state_idle)
      throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");
  }

  lock_and_set_latest_event(new_state);

  if (new_state == tracker::TrackerState::EVENT_STOPPED)
    return;

  m_dht_state = state_searching;

  if (!manager->dht_controller()->is_active())
    return receive_failed("DHT server not active.");

  manager->dht_controller()->announce(info().info_hash, this);

  state().set_normal_interval(20 * 60);
  state().set_min_interval(0);
}

void
TrackerDht::send_scrape() {
  throw internal_error("Tracker type DHT does not support scrape.");
}

void
TrackerDht::close() {
  m_slot_close();

  if (m_dht_state != state_idle)
    manager->dht_controller()->cancel_announce(&info().info_hash, this);
}

tracker_enum
TrackerDht::type() const {
  return TRACKER_DHT;
}

void
TrackerDht::receive_peers(raw_list peers) {
  if (m_dht_state == state_idle)
    throw internal_error("TrackerDht::receive_peers called while not busy.");

  m_peers.parse_address_bencode(peers);
}

void
TrackerDht::receive_success() {
  if (m_dht_state == state_idle)
    throw internal_error("TrackerDht::receive_success called while not busy.");

  m_dht_state = state_idle;

  m_slot_success(std::move(m_peers));
}

void
TrackerDht::receive_failed(const char* msg) {
  if (m_dht_state == state_idle)
    throw internal_error("TrackerDht::receive_failed called while not busy.");

  m_dht_state = state_idle;

  m_slot_failure(msg);
  m_peers.clear();
}

void
TrackerDht::receive_progress(int replied, int contacted) {
  if (m_dht_state == state_idle)
    throw internal_error("TrackerDht::receive_status called while not busy.");

  m_replied = replied;
  m_contacted = contacted;
}

} // namespace torrent
