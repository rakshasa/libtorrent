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
  m_group(0),
  m_url(url),

  m_normal_interval(0),
  m_min_interval(0),

  m_latest_event(EVENT_NONE),
  m_latest_new_peers(0),
  m_latest_sum_peers(0),

  m_success_time_last(0),
  m_success_counter(0),

  m_failed_time_last(0),
  m_failed_counter(0),

  m_scrape_time_last(0),
  m_scrape_counter(0),

  m_scrape_complete(0),
  m_scrape_incomplete(0),
  m_scrape_downloaded(0),

  m_request_time_last(torrent::cachedTime.seconds()),
  m_request_counter(0)
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

uint32_t
Tracker::success_time_next() const {
  if (m_success_counter == 0)
    return 0;

  return m_success_time_last + std::max(m_normal_interval, (uint32_t)min_normal_interval);
}

uint32_t
Tracker::failed_time_next() const {
  if (m_failed_counter == 0)
    return 0;

  if (m_min_interval > min_min_interval)
    return m_failed_time_last + m_min_interval;

  return m_failed_time_last + std::min(5 << std::min(m_failed_counter - 1, (uint32_t)6), min_min_interval-1);
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
Tracker::clear_stats() {
  m_latest_new_peers = 0;
  m_latest_sum_peers = 0;

  m_success_counter = 0;
  m_failed_counter = 0;
  m_scrape_counter = 0;
}

}
