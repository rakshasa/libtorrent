#include "config.h"

#include "rate.h"
#include "exceptions.h"

#include <algorithm>
#include <cstddef>

namespace torrent {

Rate::Rate(timer_type span) :
  Rate(span, 3, 3) {
}

Rate::Rate(timer_type span, timer_type startup_span) :
  Rate(span, startup_span, 3) {
}

Rate::Rate(timer_type span, timer_type startup_span, timer_type min_active_seconds) :
  m_span(std::max(timer_type{1}, span)),
  m_startup_span(std::max(timer_type{1}, std::min(startup_span, m_span))),
  m_min_active_seconds(std::max(timer_type{1}, std::min(min_active_seconds, m_span))),
  m_buckets(m_span, 0) {
}

size_t
Rate::bucket_index(timer_type second) const {
  return static_cast<size_t>(second) % m_buckets.size();
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

  const auto elapsed = now - m_last_second;

  if (elapsed == 0)
    return;

  if (elapsed >= m_span) {
    clear_rate();
    m_last_second = now;
    m_has_last_second = true;
    return;
  }

  for (timer_type offset = 1; offset <= elapsed; ++offset) {
    auto& bucket = m_buckets[bucket_index(m_last_second + offset)];
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
  const auto now = static_cast<timer_type>(this_thread::cached_seconds().count());
  advance_to(now);

  if (m_current == 0)
    return 0;

  const auto active_seconds = static_cast<uint64_t>(now - m_start) + 1;
  const auto divisor = std::max<uint64_t>(m_min_active_seconds, std::min<uint64_t>(m_span, active_seconds));

  return m_current / divisor;
}

void
Rate::set_span(timer_type span) {
  m_span = std::max(timer_type{1}, span);
  m_startup_span = std::min(m_startup_span, m_span);
  m_min_active_seconds = std::min(m_min_active_seconds, m_span);
  m_buckets.assign(m_span, 0);
  clear_rate();
}

void
Rate::insert(rate_type bytes) {
  const auto now = static_cast<timer_type>(this_thread::cached_seconds().count());
  advance_to(now);

  if (m_current > (rate_type{1} << 40) || bytes > (rate_type{1} << 28))
    throw internal_error("Rate::insert(bytes) received out-of-bounds values..");

  if (bytes == 0)
    return;

  // A long pause starts a fresh active period so idle time and old samples
  // cannot dilute the rate after activity resumes.
  if (m_has_last_insert &&
      now - m_last_insert >= m_startup_span) {
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
