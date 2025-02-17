#include "config.h"

#include "torrent/tracker_list.h"
#include "tracker/tracker_worker.h"

namespace torrent {

TrackerWorker::TrackerWorker(TrackerList* parent, const std::string& url, int flags) :
  m_flags(flags),
  m_parent(parent),
  m_url(url) {
}

bool
TrackerWorker::enable() {
  if ((m_flags & flag_enabled))
    return false;

  m_flags |= flag_enabled;

  return true;
}

bool
TrackerWorker::disable() {
  if (!(m_flags & flag_enabled))
    return false;

  close();
  m_flags &= ~flag_enabled;

  return true;
}

void
TrackerWorker::clear_intervals() {
  std::lock_guard<std::mutex> lock(m_state_mutex);

  m_state.m_normal_interval = 0;
  m_state.m_min_interval = 0;
}

void
TrackerWorker::clear_stats() {
  std::lock_guard<std::mutex> lock(m_state_mutex);

  m_state.m_latest_new_peers = 0;
  m_state.m_latest_sum_peers = 0;
  m_state.m_success_counter = 0;
  m_state.m_failed_counter = 0;
  m_state.m_scrape_counter = 0;
}

void
TrackerWorker::set_latest_event(TrackerState::event_enum event) {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  m_state.m_latest_event = event;
}

void
TrackerWorker::update_tracker_id(const std::string& id) {
  if (id.empty())
    return;

  std::lock_guard<std::mutex> lock(m_state_mutex);

  if (m_tracker_id == id)
    return;

  m_tracker_id = id;
}

}  // namespace torrent
