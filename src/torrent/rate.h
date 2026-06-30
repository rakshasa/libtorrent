#ifndef LIBTORRENT_UTILS_RATE_H
#define LIBTORRENT_UTILS_RATE_H

#include <deque>
#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

// Keep the current rate count up to date for each call to rate() and
// insert(...). This requires a mutable since rate() can be const, but
// is justified as we avoid iterating the deque for each call.

class Rate {
public:
  using timer_type = int32_t;
  using rate_type  = uint64_t;
  using total_type = uint64_t;

  using value_type = std::pair<timer_type, rate_type>;
  using queue_type = std::deque<value_type>;

  Rate(timer_type span) :  m_span(span) {}

  // Bytes per second.
  rate_type           rate() const;

  // Total bytes transfered.
  total_type          total() const                           { return m_total; }
  void                set_total(total_type bytes)             { m_total = bytes; }

  // Interval in seconds used to calculate the rate.
  timer_type          span() const                            { return m_span; }
  void                set_span(timer_type s)                  { m_span = s; }

  void                insert(rate_type bytes);

  void                reset_rate()                            { m_current = 0; m_container.clear(); }

  bool                operator <  (Rate& r) const             { return rate() < r.rate(); }
  bool                operator >  (Rate& r) const             { return rate() > r.rate(); }
  bool                operator == (Rate& r) const             { return rate() == r.rate(); }
  bool                operator != (Rate& r) const             { return rate() != r.rate(); }

  bool                operator <  (rate_type r) const         { return rate() < r; }
  bool                operator >  (rate_type r) const         { return rate() > r; }
  bool                operator == (rate_type r) const         { return rate() == r; }
  bool                operator != (rate_type r) const         { return rate() != r; }

private:
  inline void         discard_old() const;

  mutable queue_type  m_container;

  mutable rate_type   m_current{0};
  total_type          m_total{0};
  timer_type          m_span;
};

} // namespace torrent

#endif
