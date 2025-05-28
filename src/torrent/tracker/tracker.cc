#include "config.h"

#include "torrent/tracker/tracker.h"

#include "tracker/tracker_worker.h"

namespace torrent::tracker {

// TODO: Handle !is_valid() errors.

Tracker::Tracker(std::shared_ptr<torrent::TrackerWorker>&& worker) :
  m_worker(std::move(worker)) {
}

bool
Tracker::is_busy() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->is_busy();
}

bool
Tracker::is_busy_not_scrape() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->is_busy_not_scrape();
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

  return m_worker->is_usable();
}

bool
Tracker::is_scrapable() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->m_state.is_scrapable();
}

bool
Tracker::can_request_state() const {
  auto lock_guard = m_worker->lock_guard();

  if (!m_worker->is_usable())
    return false;

  return !m_worker->is_busy_not_scrape();
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
  m_worker->close();

  if (m_worker->m_slot_disabled)
    m_worker->m_slot_disabled();
}

tracker_enum
Tracker::type() const {
  return m_worker->type();
}

const std::string&
Tracker::url() const {
  return m_worker->info().url;
}

std::string
Tracker::tracker_id() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->tracker_id();
}

uint32_t
Tracker::group() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->group();
}

tracker::TrackerState
Tracker::state() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->state();
}

std::string
Tracker::status() const {
  return m_worker->lock_and_status();
}

void
Tracker::lock_and_call_state(const std::function<void(const TrackerState&)>& f) const {
  auto lock_guard = m_worker->lock_guard();

  f(m_worker->state());
}

void
Tracker::clear_stats() {
  m_worker->lock_and_clear_stats();
}

} // namespace torrent::tracker
