#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rate.h"
#include "settings.h"

namespace torrent {

unsigned int Rate::rate() {
  while (!m_entries.empty() &&
	 m_entries.back().first + Settings::rateWindow < Timer::cache()) {
    m_new = false;
    m_entries.pop_back();
  }
  
  if (m_entries.empty())
    return 0;

  int64_t window = m_new ?
    std::max(Timer::cache().usec() - m_entries.rbegin()->first.usec(),
	     (int64_t)Settings::rateStart) :
    Settings::rateWindow;

  int64_t bytes = 0;

  // TODO: Keep an up to date count cached.
  for (std::deque<std::pair<Timer, int> >::const_iterator itr = m_entries.begin();
       itr != m_entries.end(); ++itr)
    bytes += itr->second;

  return (bytes * 1000000) / window;
}

unsigned int Rate::rate_quick() {
  if (m_entries.empty())
    return 0;

  Timer window = Timer::cache() - Settings::rateQuick;
  int64_t bytes = 0;

  for (std::deque<std::pair<Timer, int> >::const_iterator itr = m_entries.begin();
       itr != m_entries.end() && itr->first >= window; ++itr)
    bytes += itr->second;

  return (bytes * 1000000) / Settings::rateQuick;
}

void Rate::add(unsigned int bytes) {
  // We don't need to clean up old entries here since bytes() is called
  // every 30 seconds by Download::CHOKE_CYCLE.
  m_bytes += bytes;

  if (m_entries.empty())
    m_new = true;

  if (!m_entries.empty() &&
      (Timer::cache().usec() / Settings::rateSample) == (m_entries.front().first.usec() / Settings::rateSample))
    m_entries.front().second += bytes;
    
  else
    m_entries.push_front(std::make_pair(Timer::cache(), bytes));
}

}
