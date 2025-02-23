#ifndef LIBTORRENT_HELPER_TRACKER_TEST_H
#define LIBTORRENT_HELPER_TRACKER_TEST_H

#include "torrent/tracker_list.h"
#include "tracker/tracker_worker.h"

class TrackerTest : public torrent::TrackerWorker {
public:
  static const int flag_close_on_done = max_flag_size << 0;
  static const int flag_scrape_on_success = max_flag_size << 1;

  // TODO: Clean up tracker related enums.
  TrackerTest(const torrent::TrackerInfo& info, int flags = torrent::TrackerWorker::flag_enabled) :
    torrent::TrackerWorker(info, flags),
    m_busy(false),
    m_open(false),
    m_requesting_state(-1) { m_flags |= flag_close_on_done; }

  virtual bool        is_busy() const { return m_busy; }
  bool                is_open() const { return m_open; }

  virtual torrent::tracker_enum type() const { return (torrent::tracker_enum)(torrent::TRACKER_DHT + 1); }

  int                 requesting_state() const { return m_requesting_state; }

  bool                trigger_success(uint32_t new_peers = 0, uint32_t sum_peers = 0);
  bool                trigger_success(torrent::AddressList* address_list, uint32_t new_peers = 0);
  bool                trigger_failure();
  bool                trigger_scrape();

  void                set_close_on_done(bool state) { if (state) m_flags |= flag_close_on_done; else m_flags &= ~flag_close_on_done; }
  void                set_scrape_on_success(bool state) { if (state) m_flags |= flag_scrape_on_success; else m_flags &= ~flag_scrape_on_success; }
  void                set_can_scrape()              { m_flags |= flag_can_scrape; }

  void                set_success(uint32_t counter, uint32_t time_last);
  void                set_failed(uint32_t counter, uint32_t time_last);
  void                set_latest_new_peers(uint32_t peers);
  void                set_latest_sum_peers(uint32_t peers);

  void                set_new_normal_interval(uint32_t timeout);
  void                set_new_min_interval(uint32_t timeout);

  virtual void        send_event(torrent::tracker::TrackerState::event_enum new_state);
  virtual void        send_scrape();
  virtual void        close()               { m_busy = false; m_open = false; m_requesting_state = -1; }
  virtual void        disown()              { m_busy = false; m_open = false; m_requesting_state = -1; }

  static torrent::Tracker* new_tracker(torrent::TrackerList* parent, const std::string& url, int flags = torrent::TrackerWorker::flag_enabled);
  static TrackerTest*      test_worker(torrent::Tracker* tracker);
  static int               test_flags(torrent::Tracker* tracker);

private:
  bool                m_busy;
  bool                m_open;
  int                 m_requesting_state;
};

inline torrent::Tracker*
TrackerTest::new_tracker(torrent::TrackerList* parent, const std::string& url, int flags) {
  auto tracker_info = torrent::TrackerInfo{
    // .info_hash = m_info->hash(),
    // .obfuscated_hash = m_info->hash_obfuscated(),
    // .local_id = m_info->local_id(),
    .url = url,
    // .key = m_key
  };

  return new torrent::Tracker(parent, std::shared_ptr<torrent::TrackerWorker>(new TrackerTest(tracker_info, flags)));
}

inline TrackerTest*
TrackerTest::test_worker(torrent::Tracker* tracker) {
  return dynamic_cast<TrackerTest*>(tracker->get());
}

inline int
TrackerTest::test_flags(torrent::Tracker* tracker) {
  return dynamic_cast<TrackerTest*>(tracker->get())->flags();
}

extern uint32_t return_new_peers;
inline uint32_t increment_value(int* value) { (*value)++; return return_new_peers; }

inline void increment_value_void(int* value) { (*value)++; }
inline unsigned int increment_value_uint(int* value) { (*value)++; return return_new_peers; }


#endif // LIBTORRENT_HELPER_TRACKER_TEST_H
