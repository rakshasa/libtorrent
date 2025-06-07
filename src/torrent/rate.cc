#include "config.h"

#include "rate.h"
#include "exceptions.h"

namespace torrent {

inline void
Rate::discard_old() const {
  while (!m_container.empty() && m_container.back().first < this_thread::cached_seconds().count() - m_span) {
    m_current -= m_container.back().second;
    m_container.pop_back();
  }
}

Rate::rate_type
Rate::rate() const {
  discard_old();

  return m_current / m_span;
}

void
Rate::insert(rate_type bytes) {
  discard_old();

  if (m_current > (rate_type{1} << 40) || bytes > (rate_type{1} << 28))
    throw internal_error("Rate::insert(bytes) received out-of-bounds values..");

  if (m_container.empty() || m_container.front().first != this_thread::cached_seconds().count())
    m_container.emplace_front(this_thread::cached_seconds().count(), bytes);
  else
    m_container.front().second += bytes;

  m_total += bytes;
  m_current += bytes;
}

} // namespace torrent
