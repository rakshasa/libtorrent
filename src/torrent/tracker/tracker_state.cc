#include "config.h"

#include "torrent/tracker/tracker_state.h"

namespace torrent::tracker {

std::chrono::seconds
TrackerState::success_time_next() const {
  if (m_counters.success_counter == 0)
    return {};

  return m_counters.success_time_last + std::max(m_normal_interval, min_normal_interval);
}

std::chrono::seconds
TrackerState::failed_time_next() const {
  if (m_counters.failed_counter == 0)
    return std::min(m_min_interval, min_min_interval);

  if (m_min_interval > min_min_interval)
    return m_counters.failed_time_last + m_min_interval;

  auto shift = std::min(m_counters.failed_counter - 1, uint32_t(6));

  return m_counters.failed_time_last + std::min((5 << shift) * 1s, min_min_interval);
}

std::chrono::seconds
TrackerState::activity_time_last() const {
  if (m_counters.failed_counter != 0)
    return m_counters.failed_time_last;

  return m_counters.success_time_last;
}

std::chrono::seconds
TrackerState::activity_time_next() const {
  if (m_counters.failed_counter != 0)
    return failed_time_next();

  return success_time_next();
}

std::chrono::seconds
TrackerState::activity_time_next_minimum() const {
  if (m_counters.failed_counter != 0)
    return failed_time_next();

  if (m_counters.success_counter == 0)
    return {};

  return m_counters.success_time_last + std::max(m_min_interval, min_min_interval);
}

} // namespace torrent::tracker
