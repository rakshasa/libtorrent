#ifndef LIBTORRENT_RATE_H
#define LIBTORRENT_RATE_H

#include "utils/timer.h"
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

/* New rate class draft
class Rate : private std::deque<std::pair<Timer, uint32_t> > {
public:
  typedef std::deque<std::pair<Timer, uint32_t> > Base;

  using Base::value_type;

  Rate() : m_current(0), m_total(0) {}

  uint32_t rate() const;

  uint64_t total() const { return m_total; }

  void     insert(uint32_t bytes) { 
  
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
  mutable uint32_t m_current;
  uint64_t         m_total;
}
*/

}

#endif // LIBTORRENT_RATE_H
