#include "config.h"

#include <sstream>
#include <cstdio>

#include "dht/dht_router.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/dht_manager.h"
#include "torrent/exceptions.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"

#include "tracker_dht.h"

#include "globals.h"
#include "manager.h"

namespace torrent {

const char* TrackerDht::states[] = { "Idle", "Searching", "Announcing" };

bool TrackerDht::is_allowed() { return manager->dht_manager()->is_valid(); }

TrackerDht::TrackerDht(const TrackerInfo& info, int flags) :
  TrackerWorker(info, flags),
  m_dht_state(state_idle) {

  if (!manager->dht_manager()->is_valid())
    throw internal_error("Trying to add DHT tracker with no DHT manager.");
}

TrackerDht::~TrackerDht() {
  if (m_dht_state != state_idle)
    manager->dht_manager()->router()->cancel_announce(NULL, this);
}

bool
TrackerDht::is_busy() const {
  return m_dht_state != state_idle;
}

bool
TrackerDht::is_usable() const {
  return state().is_enabled() && manager->dht_manager()->is_active();
}

void
TrackerDht::send_event(tracker::TrackerState::event_enum new_state) {
  if (m_dht_state != state_idle) {
    manager->dht_manager()->router()->cancel_announce(&info().info_hash, this);

    if (m_dht_state != state_idle)
      throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");
  }

  lock_and_set_latest_event(new_state);

  if (new_state == tracker::TrackerState::EVENT_STOPPED)
    return;

  m_dht_state = state_searching;

  if (!manager->dht_manager()->is_active())
    return receive_failed("DHT server not active.");

  manager->dht_manager()->router()->announce(info().info_hash, this);

  state().set_normal_interval(20 * 60);
  state().set_min_interval(0);
}

void
TrackerDht::send_scrape() {
  throw internal_error("Tracker type DHT does not support scrape.");
}

void
TrackerDht::close() {
  if (m_dht_state != state_idle)
    manager->dht_manager()->router()->cancel_announce(&info().info_hash, this);
}

void
TrackerDht::disown() {
  close();
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

void
TrackerDht::get_status(char* buffer, int length) {
  if (m_dht_state == state_idle)
    throw internal_error("TrackerDht::get_status called while not busy.");

  snprintf(buffer, length, "[%s: %d/%d nodes replied]", states[m_dht_state], m_replied, m_contacted);
}

}
