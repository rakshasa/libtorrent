#ifndef LIBTORRENT_RATE_H
#define LIBTORRENT_RATE_H

#include "timer.h"
#include <list>

namespace torrent {

// Assumes Timer::cache() is up to date.
class Rate {
 public:
  Rate() : m_new(true), m_bytes(0) {}

  // Bytes per second. Not using default parm since it borks my algorithms.
  // quick - Zero the rate quickly.
  int rate() { return rate(false); }
  int rate(bool quick);

  uint64_t bytes() const { return m_bytes; }

  void add(unsigned int bytes) {
    if (m_entries.empty())
      m_new = true;

    // We don't need to clean up old entries here since bytes() is called
    // every 30 seconds by Download::CHOKE_CYCLE.
    m_entries.push_front(std::make_pair(Timer::cache(), bytes));
    m_bytes += bytes;
  }

  bool operator < (Rate& r) {
    return rate() < r.rate();
  }

  bool operator > (Rate& r) {
    return rate() > r.rate();
  }

  // TODO: Make this less accurate?
  bool operator == (Rate& r) {
    return rate() == r.rate();
  }

 private:
  bool m_new;
  uint64_t m_bytes;

  std::list<std::pair<Timer, int> > m_entries;
};

}

#endif // LIBTORRENT_RATE_H
