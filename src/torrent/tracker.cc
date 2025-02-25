#include "config.h"

#include <algorithm>

#include "torrent/exceptions.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"
#include "tracker/tracker_worker.h"
#include "globals.h"

namespace torrent {

Tracker::Tracker(std::shared_ptr<TrackerWorker>&& worker) :
  m_request_time_last(torrent::cachedTime.seconds()),
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

  // TODO: Should this lock?
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

// locking and use &&
tracker::TrackerState
Tracker::state() const {
  auto lock_guard = m_worker->lock_guard();

  return m_worker->state();
}

void
Tracker::clear_stats() {
  m_worker->lock_and_clear_stats();
}

void
Tracker::inc_request_counter() {
  m_request_counter -= std::min(m_request_counter, (uint32_t)cachedTime.seconds() - m_request_time_last);
  m_request_counter++;
  m_request_time_last = cachedTime.seconds();

  if (m_request_counter >= 10)
    throw internal_error("Tracker request had more than 10 requests in 10 seconds.");
}

}
