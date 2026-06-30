#ifndef LIBTORRENT_TRACKER_TRACKER_STATE_H
#define LIBTORRENT_TRACKER_TRACKER_STATE_H

#include <algorithm>
#include <torrent/common.h>

class TrackerTest;

namespace RTORRENT_EXPORT torrent {

class TrackerDht;
class TrackerHttp;
class TrackerList;
class TrackerWorker;

namespace tracker {

class TrackerState;
class TrackerUdp;

struct TrackerParams {
  int32_t  numwant{-1};
  uint64_t uploaded_adjusted{};
  uint64_t completed_adjusted{};
  uint64_t download_left{};
};

class TrackerCounters {
private:
  friend class TrackerState;

  std::chrono::seconds success_time_last{};
  uint32_t             success_counter{};

  std::chrono::seconds failed_time_last{};
  uint32_t             failed_counter{};

  std::chrono::seconds scrape_time_last{};
  uint32_t             scrape_counter{};
};

class TrackerState {
public:
  enum event_enum {
    EVENT_NONE,
    EVENT_COMPLETED,
    EVENT_STARTED,
    EVENT_STOPPED,
    EVENT_SCRAPE
  };

  static constexpr int flag_enabled          = 0x1;
  static constexpr int flag_deleted          = 0x2;
  static constexpr int flag_requesting       = 0x4;
  static constexpr int flag_starting_request = 0x8;
  static constexpr int flag_extra_tracker    = 0x10;
  static constexpr int flag_scrapable        = 0x20;
  static constexpr int flag_disownable       = 0x40;

  static constexpr std::chrono::seconds default_min_interval    = 600s;
  static constexpr std::chrono::seconds min_min_interval        = 300s;
  static constexpr std::chrono::seconds max_min_interval        = 4 * 3600s;

  static constexpr std::chrono::seconds default_normal_interval = 1800s;
  static constexpr std::chrono::seconds min_normal_interval     = 600s;
  static constexpr std::chrono::seconds max_normal_interval     = 8 * 3600s;

  int                 flags() const               { return m_flags; }

  bool                is_enabled() const          { return (m_flags & flag_enabled); }
  bool                is_deleted() const          { return (m_flags & flag_deleted); }
  bool                is_requesting() const       { return (m_flags & flag_requesting); }
  bool                is_starting_request() const { return (m_flags & flag_starting_request); }
  bool                is_extra_tracker() const    { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const           { return is_enabled() && m_counters.success_counter != 0; }
  bool                is_scrapable() const        { return (m_flags & flag_scrapable); }
  bool                is_disownable() const       { return (m_flags & flag_disownable); }

  std::chrono::seconds normal_interval() const    { return m_normal_interval; }
  std::chrono::seconds min_interval() const       { return m_min_interval; }

  event_enum           latest_event() const       { return m_latest_event; }
  uint32_t             latest_new_peers() const   { return m_latest_new_peers; }
  uint32_t             latest_sum_peers() const   { return m_latest_sum_peers; }

  std::chrono::seconds success_time_next() const;
  std::chrono::seconds success_time_last() const  { return m_counters.success_time_last; }
  uint32_t             success_counter() const    { return m_counters.success_counter; }

  std::chrono::seconds failed_time_next() const;
  std::chrono::seconds failed_time_last() const   { return m_counters.failed_time_last; }
  uint32_t             failed_counter() const     { return m_counters.failed_counter; }

  std::chrono::seconds activity_time_last() const;
  std::chrono::seconds activity_time_next() const;
  std::chrono::seconds activity_time_next_minimum() const;

  std::chrono::seconds scrape_time_last() const   { return m_counters.scrape_time_last; }
  uint32_t             scrape_counter() const     { return m_counters.scrape_counter; }

  uint32_t             scrape_complete() const    { return m_scrape_complete; }
  uint32_t             scrape_incomplete() const  { return m_scrape_incomplete; }
  uint32_t             scrape_downloaded() const  { return m_scrape_downloaded; }

protected:
  friend class TrackerUdp;
  friend class torrent::TrackerDht;
  friend class torrent::TrackerHttp;
  friend class torrent::TrackerList;
  friend class torrent::TrackerWorker;
  friend class torrent::tracker::Tracker;
  friend class ::TrackerTest;

  void                clear_stats();

  void                set_normal_interval(std::chrono::seconds v);
  void                set_min_interval(std::chrono::seconds v);

  void                add_success_request(std::chrono::seconds time);
  void                add_failed_request(std::chrono::seconds time);
  void                add_scrape_request(std::chrono::seconds time);

  int                 m_flags{};

  std::chrono::seconds m_normal_interval{min_normal_interval};
  std::chrono::seconds m_min_interval{min_min_interval};

  event_enum          m_latest_event{EVENT_NONE};
  uint32_t            m_latest_new_peers{};
  uint32_t            m_latest_new_peers_delta{};
  uint32_t            m_latest_sum_peers{};

  TrackerCounters     m_counters;

  uint32_t            m_scrape_complete{};
  uint32_t            m_scrape_incomplete{};
  uint32_t            m_scrape_downloaded{};
};

inline void
TrackerState::clear_stats() {
  m_latest_new_peers = 0;
  m_latest_sum_peers = 0;

  m_counters.success_counter = 0;
  m_counters.failed_counter = 0;
  m_counters.scrape_counter = 0;
}

inline void
TrackerState::set_normal_interval(std::chrono::seconds v) {
  m_normal_interval = std::min(std::max(min_normal_interval, v), max_normal_interval);
}

inline void
TrackerState::set_min_interval(std::chrono::seconds v) {
  m_min_interval = std::min(std::max(min_min_interval, v), max_min_interval);
}

inline void
TrackerState::add_success_request(std::chrono::seconds time) {
  m_counters.success_time_last = time;
  m_counters.success_counter++;
  m_counters.failed_counter = 0;
}

inline void
TrackerState::add_failed_request(std::chrono::seconds time) {
  m_counters.failed_time_last = time;
  m_counters.failed_counter++;
}

inline void
TrackerState::add_scrape_request(std::chrono::seconds time) {
  m_counters.scrape_time_last = time;
  m_counters.scrape_counter++;
}

} // namespace tracker

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_STATE_H
