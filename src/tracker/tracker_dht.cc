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

TrackerDht::TrackerDht(TrackerList* parent, const std::string& url, int flags) :
  Tracker(parent, url, flags),
  m_dht_state(state_idle) {

  if (!manager->dht_manager()->is_valid())
    throw internal_error("Trying to add DHT tracker with no DHT manager.");
}

TrackerDht::~TrackerDht() {
  if (is_busy())
    manager->dht_manager()->router()->cancel_announce(NULL, this);
}

bool
TrackerDht::is_busy() const {
  return m_dht_state != state_idle;
}

bool
TrackerDht::is_usable() const {
  return is_enabled() && manager->dht_manager()->is_active();
}

void
TrackerDht::send_state(int new_state) {
  if (m_parent == NULL)
    throw internal_error("TrackerDht::send_state(...) does not have a valid m_parent.");

  if (is_busy()) {
    manager->dht_manager()->router()->cancel_announce(m_parent->info(), this);

    if (is_busy())
      throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");
  }

  set_latest_event(new_state);

  if (new_state == DownloadInfo::STOPPED) {
    return;
  }

  m_dht_state = state_searching;

  if (!manager->dht_manager()->is_active())
    return receive_failed("DHT server not active.");

  manager->dht_manager()->router()->announce(m_parent->info(), this);

  auto tracker_state = state();
  tracker_state.set_normal_interval(20 * 60);
  tracker_state.set_min_interval(0);
  set_state(tracker_state);
}

void
TrackerDht::close() {
  if (is_busy())
    manager->dht_manager()->router()->cancel_announce(m_parent->info(), this);
}

void
TrackerDht::disown() {
  close();
}

TrackerDht::Type
TrackerDht::type() const {
  return TRACKER_DHT;
}

void
TrackerDht::receive_peers(raw_list peers) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_peers called while not busy.");

  m_peers.parse_address_bencode(peers);
}

void
TrackerDht::receive_success() {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_success called while not busy.");

  m_dht_state = state_idle;
  m_parent->receive_success(this, &m_peers);
  m_peers.clear();
}

void
TrackerDht::receive_failed(const char* msg) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_failed called while not busy.");

  m_dht_state = state_idle;
  m_parent->receive_failed(this, msg);
  m_peers.clear();
}

void
TrackerDht::receive_progress(int replied, int contacted) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_status called while not busy.");

  m_replied = replied;
  m_contacted = contacted;
}

void
TrackerDht::get_status(char* buffer, int length) {
  if (!is_busy())
    throw internal_error("TrackerDht::get_status called while not busy.");

  snprintf(buffer, length, "[%s: %d/%d nodes replied]", states[m_dht_state], m_replied, m_contacted);
}

}
