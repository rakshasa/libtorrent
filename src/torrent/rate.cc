#include "config.h"

#include "torrent/rate.h"

#include <algorithm>
#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/utils/scope.h"

namespace torrent {

Rate::Rate(timer_type span) :
  Rate(span, 3, 3, span / 3) {
}

Rate::Rate(timer_type span, timer_type startup_span) :
  Rate(span, startup_span, 3, span / 3) {
}

Rate::Rate(timer_type span, timer_type startup_span, timer_type min_active_seconds, timer_type idle_timeout) :
  m_span(span),
  m_startup_span(startup_span),
  m_min_active_seconds(min_active_seconds),
  m_idle_timeout(idle_timeout),

  m_buckets(m_span, 0) {

  assert(m_span > 0);
  assert(m_startup_span > 0);
  assert(m_min_active_seconds > 0);
  assert(m_idle_timeout > 0);

  assert(m_startup_span <= m_span);
  assert(m_min_active_seconds <= m_span);
  assert(m_idle_timeout <= m_span);
}

void
Rate::clear_rate() const {
  if (m_current != 0)
    std::fill(m_buckets.begin(), m_buckets.end(), 0);

  m_current      = 0;
  m_start_insert = 0;
  m_last_insert  = 0;
  m_last_query   = 0;
}

void
Rate::advance_to(timer_type now) const {
  if (now == 0)
    throw internal_error("Rate::advance_to(now) called with now==0.");

  utils::scope_exit guard([this, now]() { m_last_query = now; });

  if (m_last_query == 0) {
    assert(m_current == 0);
    assert(m_start_insert == 0);
    return;
  }

  if (now < m_last_query) {
    clear_rate();
    return;
  }

  if (m_last_insert != 0 && now - m_last_insert > m_idle_timeout) {
    clear_rate();
    return;
  }

  const auto elapsed = now - m_last_query;

  if (elapsed == 0)
    return;

  if (elapsed >= m_span) {
    clear_rate();
    return;
  }

  for (timer_type offset = 1; offset <= elapsed; ++offset) {
    auto& bucket = m_buckets[bucket_index(m_last_query + offset)];

    m_current -= bucket;
    bucket = 0;
  }

  if (m_current == 0) {
    m_start_insert = 0;
    m_last_insert  = 0;
  }
}

Rate::rate_type
Rate::rate() const {
  // We don't care about overflow as it is far in the future.
  timer_type now = this_thread::cached_seconds().count();

  advance_to(now);

  if (m_current == 0)
    return 0;

  assert(now >= m_start_insert);

  auto active_seconds = (now - m_start_insert) + 1;
  auto divisor        = std::max(m_min_active_seconds, std::min(m_span, active_seconds));

  return m_current / divisor;
}

void
Rate::insert(rate_type bytes) {
  timer_type now = this_thread::cached_seconds().count();

  advance_to(now);

  if (bytes == 0)
    return;

  if (m_current > (rate_type{1} << 40) || bytes > (rate_type{1} << 28))
    throw internal_error("Rate::insert(bytes) received out-of-bounds values..");

  // A long pause starts a fresh active period so idle time and old samples cannot dilute the rate
  // after activity resumes.
  if (m_last_insert != 0 && now - m_last_insert > m_startup_span) {
    clear_rate();
    advance_to(now);
  }

  if (m_start_insert == 0)
    m_start_insert = now;

  auto& bucket = m_buckets[bucket_index(now)];

  bucket    += bytes;
  m_current += bytes;
  m_total   += bytes;

  m_last_insert = now;
}

void
Rate::reset_rate() {
  clear_rate();
}

} // namespace torrent
