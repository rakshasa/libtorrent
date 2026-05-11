#include "config.h"

#include "torrent/tracker/tracker.h"

#include "tracker/tracker_dht.h"
#include "tracker/tracker_worker.h"
#include "torrent/runtime/network_manager.h"

namespace torrent::tracker {

// TODO: Handle !is_valid() errors.

Tracker::Tracker(std::shared_ptr<torrent::TrackerWorker>&& worker) :
  m_worker(std::move(worker)) {
}

bool
Tracker::is_busy() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_requesting();
}

bool
Tracker::is_busy_not_scrape() const {
  auto lock_guard = m_worker->lock_guard();

  if (m_worker->m_state.is_requesting())
    return m_worker->m_state.latest_event() != tracker::TrackerState::EVENT_SCRAPE;

  return false;
}

bool
Tracker::is_enabled() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_enabled();
}

bool
Tracker::is_extra_tracker() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_extra_tracker();
}

bool
Tracker::is_in_use() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_in_use();
}

bool
Tracker::is_usable() const {
  auto lock_guard = m_worker->lock_guard();

  if (m_worker->type() == tracker_enum::TRACKER_DHT && !runtime::network_manager()->is_dht_active())
    return false;

  return m_worker->m_state.is_enabled();
}

bool
Tracker::is_scrapable() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_scrapable();
}

bool
Tracker::can_request_state() const {
  auto lock_guard = m_worker->lock_guard();

  if (m_worker->type() == tracker_enum::TRACKER_DHT && !runtime::network_manager()->is_dht_active())
    return false;

  if (!m_worker->m_state.is_enabled())
    return false;

  if (m_worker->m_state.is_requesting())
    return m_worker->m_state.latest_event() == tracker::TrackerState::EVENT_SCRAPE;

  return true;
}

tracker_enum
Tracker::type() const {
  return m_worker->type();
}

const std::string&
Tracker::url() const {
  return m_worker->info().url;
}

uint32_t
Tracker::group() const {
  return m_worker->info().group;
}

std::string
Tracker::tracker_id() const {
  return m_worker->tracker_id_safe();
}

void
Tracker::enable() {
  {
    auto lock_guard = m_worker->lock_guard();

    if (m_worker->m_state.is_enabled())
      return;

    m_worker->m_state.m_flags |= tracker::TrackerState::flag_enabled;
  }

  if (m_worker->m_slot_enabled)
    m_worker->m_slot_enabled();
}

void
Tracker::disable() {
  {
    auto lock_guard = m_worker->lock_guard();

    if (!m_worker->m_state.is_enabled())
      return;

    m_worker->m_state.m_flags &= ~tracker::TrackerState::flag_enabled;
  }

  // TODO: This should be called through manager? It works atm as the trackers are all running on main
  // thread.
  // m_worker->close();

  if (m_worker->m_slot_disabled)
    m_worker->m_slot_disabled();
}

tracker::TrackerState
Tracker::state() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->state();
}

std::string
Tracker::status() const {
  auto lock_guard = m_worker->lock_guard();

  if (m_worker->type() != tracker_enum::TRACKER_DHT)
    return "";

  auto tracker = dynamic_cast<TrackerDht*>(m_worker.get());

  return "[" + std::string(TrackerDht::states[tracker->dht_state()]) + ": " + std::to_string(tracker->replied()) + "/" + std::to_string(tracker->contacted()) + " nodes replied]";
}

void
Tracker::lock_and_call_state(const std::function<void(const TrackerState&)>& fn) const {
  auto lock_guard = m_worker->lock_guard();

  fn(m_worker->state());
}

void
Tracker::clear_stats() {
  m_worker->lock_and_clear_stats();
}

} // namespace torrent::tracker
