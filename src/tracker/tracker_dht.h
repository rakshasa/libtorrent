#ifndef LIBTORRENT_TRACKER_TRACKER_DHT_H
#define LIBTORRENT_TRACKER_TRACKER_DHT_H

#include <array>

#include "net/address_list.h"
#include "torrent/object.h"
#include "torrent/utils/scheduler.h"
#include "tracker/tracker_worker.h"

namespace torrent {

// Until we make throttle and rate thread-safe, we keep dht router in main thread.
//
// Both implemenation will require that interacting with dht router is thread safe, so we still need
// to lock dht router.

class TrackerDht : public TrackerWorker {
public:
  TrackerDht(const TrackerInfo& info, int flags = 0);
  ~TrackerDht() override;

  enum state_type {
    state_idle,
    state_searching,
    state_announcing,
  };

  static constexpr std::array states{ "Idle", "Searching", "Announcing" };

  tracker_enum        type() const override;

  static bool         is_allowed();

  std::string         lock_and_status() const override;

  void                send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) override;
  void                send_scrape(tracker::TrackerParams params) override;

  void                close() override;

  state_type          get_dht_state() const            { return m_dht_state; }
  void                set_dht_announce_state();

  bool                has_peers() const                { return !m_peers.empty(); }

  void                receive_peers(raw_list peers);
  void                receive_success();
  void                receive_failed(const char* msg);
  void                receive_progress(int replied, int contacted);

private:
  void                update_requesting_state();

  tracker::TrackerParams m_params;

  AddressList         m_peers;
  state_type          m_dht_state{state_idle};

  int                 m_replied;
  int                 m_contacted;

  utils::SchedulerEntry m_delay_clear_state;
};

} // namespace torrent

#endif
