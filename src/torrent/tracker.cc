#include "config.h"

#include <algorithm>

#include "torrent/exceptions.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"
#include "tracker/tracker_worker.h"
#include "globals.h"

namespace torrent {

Tracker::Tracker(TrackerList* parent, std::shared_ptr<TrackerWorker>&& worker) :
  m_parent(parent),
  m_request_time_last(torrent::cachedTime.seconds()),
  m_worker(std::move(worker)) {
}

bool
Tracker::is_enabled() const         { return m_worker->is_enabled(); }

bool
Tracker::is_extra_tracker() const   { return m_worker->is_extra_tracker(); }

bool
Tracker::is_in_use() const          { return m_worker->is_in_use(); }

bool
Tracker::is_busy() const            { return m_worker->is_busy(); }

bool
Tracker::is_busy_not_scrape() const { return m_worker->is_busy_not_scrape(); }

bool
Tracker::is_usable() const {
  return m_worker->is_usable();
}

bool
Tracker::can_request_state() const {
  return !(m_worker->is_busy() && state().latest_event() != TrackerState::EVENT_SCRAPE) && is_usable();
}

bool
Tracker::can_scrape() const {
  return m_worker->can_scrape();
}

void
Tracker::enable() {
  if (!m_worker->enable())
    return;

  if (m_parent->slot_tracker_enabled())
    m_parent->slot_tracker_enabled()(this);
}

void
Tracker::disable() {
  if (!m_worker->disable())
    return;

  if (m_parent->slot_tracker_disabled())
    m_parent->slot_tracker_disabled()(this);
}

tracker_enum
Tracker::type() const {
  return m_worker->type();
}

const std::string&
Tracker::url() const {
  return m_worker->url();
}

std::string
Tracker::tracker_id() const {
  return m_worker->tracker_id();
}

TrackerState
Tracker::state() const {
  return m_worker->state();
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
