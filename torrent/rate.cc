#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rate.h"
#include "settings.h"

namespace torrent {

int Rate::rate(bool quick) {
  while (!m_entries.empty() &&
	 m_entries.back().first + Settings::rateWindow < Timer::cache()) {
    m_new = false;
    m_entries.pop_back();
  }
  
  if (m_entries.empty())
    return 0;

  if (quick &&
      m_entries.front().first + Settings::rateQuick < Timer::cache())
    return 0;

  int64_t window = m_new ?
    std::max(Timer::cache().usec() - m_entries.rbegin()->first.usec(),
	     (int64_t)Settings::rateStart) :
    Settings::rateWindow;

  int64_t bytes = 0;

  for (std::list<std::pair<Timer, int> >::const_iterator itr = m_entries.begin();
       itr != m_entries.end(); ++itr)
    bytes += itr->second;

  return (bytes * 1000000) / window;
}

}
