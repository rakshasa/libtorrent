#ifndef LIBTORRENT_TRACKER_TRACKER_STATE_H
#define LIBTORRENT_TRACKER_TRACKER_STATE_H

#include <algorithm>
#include <torrent/common.h>

class TrackerTest;

namespace torrent {

class TrackerDht;
class TrackerHttp;
class TrackerList;
class TrackerUdp;
class TrackerWorker;

namespace tracker {

class TrackerState {
public:
  enum event_enum {
    EVENT_NONE,
    EVENT_COMPLETED,
    EVENT_STARTED,
    EVENT_STOPPED,
    EVENT_SCRAPE
  };

  static constexpr int flag_enabled       = 0x1;
  static constexpr int flag_extra_tracker = 0x2;
  static constexpr int flag_scrapable     = 0x4;

  // TODO: Remove these:
  // static constexpr int max_flag_size   = 0x10;
  // static constexpr int mask_base_flags = 0x10 - 1;

  static constexpr int default_min_interval = 600;
  static constexpr int min_min_interval     = 300;
  static constexpr int max_min_interval     = 4 * 3600;

  static constexpr int default_normal_interval = 1800;
  static constexpr int min_normal_interval     = 600;
  static constexpr int max_normal_interval     = 8 * 3600;

  int                 flags() const              { return m_flags; }

  bool                is_enabled() const         { return (m_flags & flag_enabled); }
  bool                is_extra_tracker() const   { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const          { return is_enabled() && m_success_counter != 0; }
  bool                is_scrapable() const       { return (m_flags & flag_scrapable); }

  uint32_t            normal_interval() const    { return m_normal_interval; }
  uint32_t            min_interval() const       { return m_min_interval; }

  event_enum          latest_event() const       { return m_latest_event; }
  uint32_t            latest_new_peers() const   { return m_latest_new_peers; }
  uint32_t            latest_sum_peers() const   { return m_latest_sum_peers; }

  uint32_t            success_time_next() const;
  uint32_t            success_time_last() const  { return m_success_time_last; }
  uint32_t            success_counter() const    { return m_success_counter; }

  uint32_t            failed_time_next() const;
  uint32_t            failed_time_last() const   { return m_failed_time_last; }
  uint32_t            failed_counter() const     { return m_failed_counter; }

  uint32_t            activity_time_last() const { return failed_counter() ? m_failed_time_last : m_success_time_last; }
  uint32_t            activity_time_next() const { return failed_counter() ? failed_time_next() : success_time_next(); }

  uint32_t            scrape_time_last() const   { return m_scrape_time_last; }
  uint32_t            scrape_counter() const     { return m_scrape_counter; }

  uint32_t            scrape_complete() const    { return m_scrape_complete; }
  uint32_t            scrape_incomplete() const  { return m_scrape_incomplete; }
  uint32_t            scrape_downloaded() const  { return m_scrape_downloaded; }

protected:
  friend class torrent::TrackerDht;
  friend class torrent::TrackerHttp;
  friend class torrent::TrackerList;
  friend class torrent::TrackerUdp;
  friend class torrent::TrackerWorker;
  friend class torrent::tracker::Tracker;
  friend class ::TrackerTest;

  void                clear_intervals();
  void                clear_stats();

  void                set_normal_interval(int v);
  void                set_min_interval(int v);

  void                inc_request_counter();

  int                 m_flags;

  uint32_t            m_normal_interval{0};
  uint32_t            m_min_interval{0};

  event_enum          m_latest_event{EVENT_NONE};
  uint32_t            m_latest_new_peers{0};
  uint32_t            m_latest_new_peers_delta{0};
  uint32_t            m_latest_sum_peers{0};

  uint32_t            m_success_time_last{0};
  uint32_t            m_success_counter{0};

  uint32_t            m_failed_time_last{0};
  uint32_t            m_failed_counter{0};

  uint32_t            m_scrape_time_last{0};
  uint32_t            m_scrape_counter{0};

  uint32_t            m_scrape_complete{0};
  uint32_t            m_scrape_incomplete{0};
  uint32_t            m_scrape_downloaded{0};
};

inline uint32_t
TrackerState::success_time_next() const {
  if (m_success_counter == 0)
    return 0;

  return m_success_time_last + std::max(m_normal_interval, static_cast<uint32_t>(min_normal_interval));
}

inline uint32_t
TrackerState::failed_time_next() const {
  if (m_failed_counter == 0)
    return 0;

  if (m_min_interval > min_min_interval)
    return m_failed_time_last + m_min_interval;

  return m_failed_time_last + std::min(5 << std::min<uint32_t>(m_failed_counter - 1, 6), min_min_interval - 1);
}

inline void
TrackerState::clear_intervals() {
  m_normal_interval = 0;
  m_min_interval = 0;
}

inline void
TrackerState::clear_stats() {
  m_latest_new_peers = 0;
  m_latest_sum_peers = 0;
  m_success_counter = 0;
  m_failed_counter = 0;
  m_scrape_counter = 0;
}

inline void
TrackerState::set_normal_interval(int v) {
  m_normal_interval = std::min(std::max(min_normal_interval, v), max_normal_interval);
}

inline void
TrackerState::set_min_interval(int v) {
  m_min_interval = std::min(std::max(min_min_interval, v), max_min_interval);
}

} // namespace tracker

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_STATE_H
