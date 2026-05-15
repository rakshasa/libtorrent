#ifndef LIBTORRENT_TRACKER_TRACKER_DHT_H
#define LIBTORRENT_TRACKER_TRACKER_DHT_H

#include <array>
#include <memory>

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

  enum state_type {
    state_idle,
    state_searching,
    state_announcing,
  };

  static constexpr std::array states{ "Idle", "Searching", "Announcing" };

  tracker_enum        type() const override;

  void                send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) override;
  void                send_scrape(tracker::TrackerParams params) override;

  void                close() override;

  state_type          dht_state() const;
  void                set_dht_announce_state();

  int                 replied() const;
  int                 contacted() const;

  bool                has_peers_unsafe() const;

  void                receive_peers(raw_list peers);
  void                receive_success();
  void                receive_failed(const char* msg);
  void                receive_progress(int replied, int contacted);

private:
  void                cleanup() override;

  void                update_requesting_state();

  tracker::TrackerParams  m_params;
  AddressList             m_peers;

  std::atomic<state_type> m_dht_state{state_idle};

  std::atomic<int>        m_replied;
  std::atomic<int>        m_contacted;

  utils::SchedulerEntry   m_delay_clear_state;
};

inline TrackerDht::state_type TrackerDht::dht_state() const        { return m_dht_state; }
inline int                    TrackerDht::replied() const          { return m_replied; }
inline int                    TrackerDht::contacted() const        { return m_contacted; }
inline bool                   TrackerDht::has_peers_unsafe() const { return !m_peers.empty(); }

} // namespace torrent

#endif
