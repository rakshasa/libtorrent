#ifndef LIBTORRENT_RATE_H
#define LIBTORRENT_RATE_H

#include "timer.h"
#include <deque>

namespace torrent {

// TODO: We need to have a quick changeing rate class with total/average
// for the past X*k seconds and stuff like that.

// Assumes Timer::cache() is up to date.
class Rate {
 public:
  Rate() : m_new(true), m_bytes(0) {}

  unsigned int rate() const;
  unsigned int rate_quick() const;

  uint64_t total() const { return m_bytes; }

  void add(unsigned int bytes);
  
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
  mutable bool m_new;
  uint64_t m_bytes;

  mutable std::deque<std::pair<Timer, int> > m_entries;
};

}

#endif // LIBTORRENT_RATE_H
