#ifndef LIBTORRENT_UTILS_RATE_H
#define LIBTORRENT_UTILS_RATE_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

// TODO: Convert to template with std::array.

class LIBTORRENT_EXPORT Rate {
public:
  using timer_type = uint32_t;
  using rate_type  = uint64_t;
  using total_type = uint64_t;

  Rate(timer_type span);
  Rate(timer_type span, timer_type startup_span);
  Rate(timer_type span, timer_type startup_span, timer_type min_active_seconds);

  // The divisor grows from min_active_seconds after the first insert, up to span().
  // Inserts separated by startup_span() or more seconds restart that count;
  // shorter pauses remain in the active period.
  // Bytes per second.
  rate_type           rate() const;

  total_type          total() const                           { return m_total; }
  void                set_total(total_type bytes)             { m_total = bytes; }

  timer_type          span() const                            { return m_span; }
  void                set_span(timer_type s);

  timer_type          startup_span() const                    { return m_startup_span; }

  void                insert(rate_type bytes);
  void                reset_rate();

private:
  // TODO: Remove mutable after merge.

  void                advance_to(timer_type now) const;
  void                clear_rate() const;

  uint32_t            bucket_index(timer_type second) const;

  timer_type          m_span;
  timer_type          m_startup_span;
  timer_type          m_min_active_seconds;

  mutable std::vector<rate_type> m_buckets;

  mutable rate_type   m_current{};
  total_type          m_total{};

  mutable timer_type  m_last_second{};
  mutable timer_type  m_last_insert{};
  mutable timer_type  m_start{};

  mutable bool        m_has_last_second{};
  mutable bool        m_has_last_insert{};
  mutable bool        m_has_start{};
};

} // namespace torrent

#endif
