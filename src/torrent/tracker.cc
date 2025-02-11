#include "config.h"

#include <algorithm>

#include "torrent/exceptions.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"
#include "globals.h"

namespace torrent {

Tracker::Tracker(TrackerList* parent, const std::string& url, int flags) :
  m_flags(flags),
  m_parent(parent),
  m_url(url),
  m_request_time_last(torrent::cachedTime.seconds())
{
}

void
Tracker::enable() {
  if (is_enabled())
    return;

  m_flags |= flag_enabled;

  if (m_parent->slot_tracker_enabled())
    m_parent->slot_tracker_enabled()(this);
}

void
Tracker::disable() {
  if (!is_enabled())
    return;

  close();
  m_flags &= ~flag_enabled;

  if (m_parent->slot_tracker_disabled())
    m_parent->slot_tracker_disabled()(this);
}

std::string
Tracker::scrape_url_from(std::string url) {
  size_t delim_slash = url.rfind('/');

  if (delim_slash == std::string::npos || url.find("/announce", delim_slash) != delim_slash)
    throw internal_error("Tried to make scrape url from invalid url.");

  return url.replace(delim_slash, sizeof("/announce") - 1, "/scrape");
}

void
Tracker::send_scrape() {
  throw internal_error("Tracker type does not support scrape.");
}

void
Tracker::inc_request_counter() {
  m_request_counter -= std::min(m_request_counter, (uint32_t)cachedTime.seconds() - m_request_time_last);
  m_request_counter++;
  m_request_time_last = cachedTime.seconds();

  if (m_request_counter >= 10)
    throw internal_error("Tracker request had more than 10 requests in 10 seconds.");
}

void
Tracker::clear_intervals() {
  std::lock_guard<std::mutex> lock(m_state_mutex);

  m_state.m_normal_interval = 0;
  m_state.m_min_interval = 0;
}

void
Tracker::clear_stats() {
  std::lock_guard<std::mutex> lock(m_state_mutex);

  m_state.m_latest_new_peers = 0;
  m_state.m_latest_sum_peers = 0;
  m_state.m_success_counter = 0;
  m_state.m_failed_counter = 0;
  m_state.m_scrape_counter = 0;
}

void
Tracker::set_latest_event(TrackerState::event_enum event) {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  m_state.m_latest_event = event;
}

void
Tracker::update_tracker_id(const std::string& id) {
  if (id.empty())
    return;

  std::lock_guard<std::mutex> lock(m_state_mutex);

  if (m_tracker_id == id)
    return;

  m_tracker_id = id;
}

}
