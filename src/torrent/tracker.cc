#include "config.h"

#include <algorithm>

#include "exceptions.h"
#include "globals.h"
#include "tracker.h"
#include "tracker_list.h"

namespace torrent {

constexpr int Tracker::default_min_interval;
constexpr int Tracker::min_min_interval;
constexpr int Tracker::max_min_interval;
constexpr int Tracker::default_normal_interval;
constexpr int Tracker::min_normal_interval;
constexpr int Tracker::max_normal_interval;

Tracker::Tracker(TrackerList* parent, const std::string& url, int flags) :
  m_flags(flags),
  m_parent(parent),
  m_url(url),
  m_request_time_last(torrent::cachedTime.seconds())
{
  // TODO: Not needed when EVENT_NONE is default.
  auto tracker_state = TrackerState{};
  tracker_state.m_latest_event = EVENT_NONE;
  m_state.store(tracker_state);
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

inline void
Tracker::clear_intervals() {
  auto state = m_state.load();

  state.m_normal_interval = 0;
  state.m_min_interval = 0;

  m_state.store(state);
}

void
Tracker::clear_stats() {
  auto state = m_state.load();

  state.m_latest_new_peers = 0;
  state.m_latest_sum_peers = 0;
  state.m_success_counter = 0;
  state.m_failed_counter = 0;
  state.m_scrape_counter = 0;

  m_state.store(state);
}

inline void
Tracker::set_latest_event(int v) {
  auto state = m_state.load();

  state.m_latest_event = v;

  m_state.store(state);
}

void
Tracker::update_tracker_id(const std::string& id) {
  if (id.empty())
    return;

  if (tracker_id() == id)
    return;

  auto new_id = std::array<char,64>{};
  std::copy_n(id.begin(), std::min(id.size(), size_t(63)), new_id.begin());

  m_tracker_id.store(new_id);
}

}
