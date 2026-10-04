#ifndef LIBTORRENT_UTILS_RATE_H
#define LIBTORRENT_UTILS_RATE_H

#include <cstddef>
#include <vector>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT Rate {
public:
  using timer_type = int32_t;
  using rate_type  = uint64_t;
  using total_type = uint64_t;

  Rate(timer_type span);

  // The divisor grows from 1 second after the first insert, up to span().
  // Inserts separated by startup_span() or more seconds restart that count;
  // shorter pauses remain in the active period.
  // Bytes per second.
  rate_type           rate() const;

  // Total bytes transfered.
  total_type          total() const                           { return m_total; }
  void                set_total(total_type bytes)             { m_total = bytes; }

  // Interval in seconds used to calculate the rate.
  timer_type          span() const                            { return m_span; }
  void                set_span(timer_type s);

  // Pauses this long reset the active period; shorter pauses count toward its divisor.
  timer_type          startup_span() const                    { return m_startup_span; }
  void                set_startup_span(timer_type s);

  void                insert(rate_type bytes);
  void                reset_rate();

private:
  void                advance_to(timer_type now) const;
  void                clear_rate() const;
  std::size_t         bucket_index(timer_type second) const;

  timer_type          m_span;
  timer_type          m_startup_span;
  mutable std::vector<rate_type> m_buckets;
  mutable rate_type   m_current{0};
  mutable timer_type  m_last_second{0};
  mutable timer_type  m_last_insert{0};
  mutable timer_type  m_start{0};
  mutable bool        m_has_last_second{false};
  mutable bool        m_has_last_insert{false};
  mutable bool        m_has_start{false};
  total_type          m_total{0};
};

} // namespace torrent

#endif
