#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rate.h"
#include "settings.h"

namespace torrent {

inline void
Rate::discard_old() const {
  while (!m_container.empty() && m_container.back().first < Timer::cache() - timespan * 1000000) {
    m_current -= m_container.back().second;
    m_container.pop_back();
  }
}

// Assumes the container is not empty.
uint32_t
Rate::data_time_span() const {
  return (Timer::cache() - m_container.back().first).usec() / 1000000;
}

uint32_t
Rate::rate() const {
  // Do an empty check here since having a non-empty container that
  // gets emptied in discard_old() is relatively rare.
  if (m_container.empty())
    return 0;

  discard_old();

  return m_current / std::max<uint32_t>(10, data_time_span());
}

void
Rate::insert(uint32_t bytes) {
  discard_old();

  if (m_container.empty() || m_container.front().first < Timer::cache() - 1 * 1000000)
    m_container.push_front(value_type(Timer::cache(), bytes));
  else
    m_container.front().second += bytes;

  m_current += bytes;
}
  

}
