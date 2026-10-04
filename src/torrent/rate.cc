#include "config.h"

#include "rate.h"
#include "exceptions.h"

#include <algorithm>
#include <cstddef>

namespace torrent {

Rate::Rate(timer_type span) :
  m_span(std::max(timer_type{1}, span)),
  m_startup_span(std::min(m_span, timer_type{3})),
  m_buckets(m_span, 0) {
}

size_t
Rate::bucket_index(timer_type second) const {
  auto index = static_cast<int64_t>(second) % static_cast<int64_t>(m_buckets.size());

  if (index < 0)
    index += m_buckets.size();

  return static_cast<size_t>(index);
}

void
Rate::clear_rate() const {
  std::fill(m_buckets.begin(), m_buckets.end(), 0);
  m_current = 0;
  m_has_last_second = false;
  m_has_last_insert = false;
  m_has_start = false;
}

void
Rate::advance_to(timer_type now) const {
  if (!m_has_last_second) {
    m_last_second = now;
    m_has_last_second = true;
    return;
  }

  if (now < m_last_second) {
    clear_rate();
    m_last_second = now;
    m_has_last_second = true;
    return;
  }

  const auto elapsed = static_cast<int64_t>(now) - m_last_second;

  if (elapsed == 0)
    return;

  if (elapsed >= m_span) {
    clear_rate();
    m_last_second = now;
    m_has_last_second = true;
    return;
  }

  for (auto second = static_cast<int64_t>(m_last_second) + 1; second <= now; ++second) {
    auto& bucket = m_buckets[bucket_index(static_cast<timer_type>(second))];
    m_current -= bucket;
    bucket = 0;
  }

  m_last_second = now;

  if (m_current == 0) {
    m_has_last_insert = false;
    m_has_start = false;
  }
}

Rate::rate_type
Rate::rate() const {
  const auto now = this_thread::cached_seconds().count();
  advance_to(now);

  if (m_current == 0)
    return 0;

  const auto active_seconds = static_cast<int64_t>(now) - m_start + 1;
  const auto divisor = std::max<int64_t>(1, std::min<int64_t>(m_span, active_seconds));

  return m_current / divisor;
}

void
Rate::set_span(timer_type span) {
  m_span = std::max(timer_type{1}, span);
  m_startup_span = std::min(m_startup_span, m_span);
  m_buckets.assign(m_span, 0);
  clear_rate();
}

void
Rate::set_startup_span(timer_type span) {
  m_startup_span = std::max(timer_type{1}, std::min(span, m_span));
}

void
Rate::insert(rate_type bytes) {
  const auto now = this_thread::cached_seconds().count();
  advance_to(now);

  if (m_current > (rate_type{1} << 40) || bytes > (rate_type{1} << 28))
    throw internal_error("Rate::insert(bytes) received out-of-bounds values..");

  if (bytes == 0)
    return;

  // A long pause starts a fresh active period so idle time and old samples
  // cannot dilute the rate after activity resumes.
  if (m_has_last_insert &&
      static_cast<int64_t>(now) - m_last_insert >= m_startup_span) {
    clear_rate();
    advance_to(now);
  }

  if (!m_has_start) {
    m_start = now;
    m_has_start = true;
  }

  auto& bucket = m_buckets[bucket_index(now)];
  bucket += bytes;
  m_current += bytes;
  m_total += bytes;

  m_last_insert = now;
  m_has_last_insert = true;
}

void
Rate::reset_rate() {
  clear_rate();
}

} // namespace torrent
