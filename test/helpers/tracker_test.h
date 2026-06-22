#ifndef LIBTORRENT_HELPER_TRACKER_TEST_H
#define LIBTORRENT_HELPER_TRACKER_TEST_H

#include <atomic>

#include "tracker/tracker_list.h"
#include "tracker/tracker_worker.h"

class TrackerTest : public torrent::TrackerWorker {
public:
  static const int flag_close_on_done     = 0x100;
  static const int flag_scrape_on_success = 0x200;

  // TODO: Clean up tracker related enums.
  TrackerTest(torrent::TrackerInfo info, int flags = torrent::tracker::TrackerState::flag_enabled);

  bool                is_open() const          { return m_open; }

  torrent::tracker_enum type() const override  { return (torrent::tracker_enum)(torrent::TRACKER_DHT + 1); }

  int                 requesting_state() const { return m_requesting_state; }

  bool                trigger_success(uint32_t new_peers = 0, uint32_t sum_peers = 0);
  bool                trigger_success(torrent::AddressList* address_list, uint32_t new_peers = 0);
  bool                trigger_failure();
  bool                trigger_scrape();

  void                set_close_on_done(bool state);
  void                set_scrape_on_success(bool state);
  void                set_scrapable();

  void                set_success(uint32_t time_last);
  void                set_success(std::chrono::seconds time_last);
  void                set_failed(uint32_t time_last);
  void                set_failed(std::chrono::seconds time_last);
  void                set_latest_new_peers(uint32_t peers);
  void                set_latest_sum_peers(uint32_t peers);

  void                set_new_normal_interval(uint32_t timeout);
  void                set_new_min_interval(uint32_t timeout);

  void                send_event(torrent::tracker::TrackerParams params, torrent::tracker::TrackerState::event_enum new_state) override;
  void                send_scrape(torrent::tracker::TrackerParams params) override;

  void                close() override;
  void                cleanup() override;

  static torrent::tracker::Tracker       new_tracker(torrent::TrackerList* parent, uint32_t group, const std::string& url, int flags = torrent::tracker::TrackerState::flag_enabled);
  static void                            insert_tracker(torrent::TrackerList* parent, int group, torrent::tracker::Tracker tracker);

  torrent::tracker::TrackerState*        state_ptr() { return &state(); }
  torrent::tracker::TrackerState&        test_state() { return state(); }

  static TrackerTest*                    test_worker(torrent::tracker::Tracker& tracker);
  static int                             test_flags(torrent::tracker::Tracker& tracker);
  static torrent::tracker::TrackerState& test_state(torrent::tracker::Tracker& tracker);

  static int count_active(torrent::TrackerList* parent);
  static int count_usable(torrent::TrackerList* parent);

private:
  align_cacheline std::atomic<bool> m_busy{false};
  std::atomic<bool>   m_open{false};
  std::atomic<int>    m_requesting_state{-1};
};

inline
TrackerTest::TrackerTest(torrent::TrackerInfo info, int flags) :
  torrent::TrackerWorker(std::move(info), flags) {

  state().m_flags |= flag_close_on_done;
}

inline void
TrackerTest::set_close_on_done(bool s) {
  if (s)
    state().m_flags |= flag_close_on_done;
  else
    state().m_flags &= ~flag_close_on_done;
}

inline void
TrackerTest::set_scrape_on_success(bool s) {
  if (s)
    state().m_flags |= flag_scrape_on_success;
  else
    state().m_flags &= ~flag_scrape_on_success;
}

inline void
TrackerTest::set_scrapable() {
  state().m_flags |= torrent::tracker::TrackerState::flag_scrapable;
}

inline TrackerTest*
TrackerTest::test_worker(torrent::tracker::Tracker& tracker) {
  return dynamic_cast<TrackerTest*>(tracker.get_worker());
}

inline int
TrackerTest::test_flags(torrent::tracker::Tracker& tracker) {
  return dynamic_cast<TrackerTest*>(tracker.get_worker())->state().flags();
}

inline torrent::tracker::TrackerState&
TrackerTest::test_state(torrent::tracker::Tracker& tracker) {
  return dynamic_cast<TrackerTest*>(tracker.get_worker())->state();
}

extern uint32_t return_new_peers;
inline uint32_t increment_value(int* value) { (*value)++; return return_new_peers; }

inline void increment_value_void(int* value) { (*value)++; }
inline unsigned int increment_value_uint(int* value) { (*value)++; return return_new_peers; }


#endif // LIBTORRENT_HELPER_TRACKER_TEST_H
