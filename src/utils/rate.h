#ifndef LIBTORRENT_RATE_H
#define LIBTORRENT_RATE_H

#include "utils/timer.h"
#include <deque>

namespace torrent {

// Keep the current rate count up to date for each call to rate() and
// insert(...). This requires a mutable since rate() is const, but if
// justified as we don't need to iterate the deque for each call.

class Rate {
public:
  typedef std::pair<Timer, uint32_t> value_type;
  typedef std::deque<value_type>     Container;

  static const uint32_t timespan = 30;

  Rate() : m_current(0), m_total(0) {}

  uint32_t            rate() const;
  uint64_t            total() const                         { return m_total; }

  void                insert(uint32_t bytes);
  
  bool                operator < (Rate& r)                  { return rate() < r.rate(); }
  bool                operator > (Rate& r)                  { return rate() > r.rate(); }

private:
  inline void         discard_old() const;
  inline uint32_t     data_time_span() const;

  mutable Container   m_container;
  mutable uint32_t    m_current;
  uint64_t            m_total;
};

}

#endif // LIBTORRENT_RATE_H
