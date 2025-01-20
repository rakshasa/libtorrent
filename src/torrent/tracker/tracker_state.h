#ifndef LIBTORRENT_TRACKER_TRACKER_STATE_H
#define LIBTORRENT_TRACKER_TRACKER_STATE_H

#include <torrent/common.h>

namespace torrent {

class TrackerState {
public:
  friend class Tracker;

  uint32_t            normal_interval() const               { return m_normal_interval; }
  uint32_t            min_interval() const                  { return m_min_interval; }

  int                 latest_event() const                  { return m_latest_event; }
  uint32_t            latest_new_peers() const              { return m_latest_new_peers; }
  uint32_t            latest_sum_peers() const              { return m_latest_sum_peers; }

  uint32_t            success_time_next() const;
  uint32_t            success_time_last() const             { return m_success_time_last; }
  uint32_t            success_counter() const               { return m_success_counter; }

  uint32_t            failed_time_next() const;
  uint32_t            failed_time_last() const              { return m_failed_time_last; }
  uint32_t            failed_counter() const                { return m_failed_counter; }

  uint32_t            activity_time_last() const            { return failed_counter() ? m_failed_time_last : m_success_time_last; }
  uint32_t            activity_time_next() const            { return failed_counter() ? failed_time_next() : success_time_next(); }

  uint32_t            scrape_time_last() const              { return m_scrape_time_last; }
  uint32_t            scrape_counter() const                { return m_scrape_counter; }

  uint32_t            scrape_complete() const               { return m_scrape_complete; }
  uint32_t            scrape_incomplete() const             { return m_scrape_incomplete; }
  uint32_t            scrape_downloaded() const             { return m_scrape_downloaded; }

protected:
  uint32_t m_normal_interval;
  uint32_t m_min_interval;

  int      m_latest_event;
  uint32_t m_latest_new_peers;
  uint32_t m_latest_sum_peers;

  uint32_t m_success_time_last;
  uint32_t m_success_counter;

  uint32_t m_failed_time_last;
  uint32_t m_failed_counter;

  uint32_t m_scrape_time_last;
  uint32_t m_scrape_counter;

  uint32_t m_scrape_complete;
  uint32_t m_scrape_incomplete;
  uint32_t m_scrape_downloaded;
};

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_STATE_H
