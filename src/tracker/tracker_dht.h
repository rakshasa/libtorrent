#ifndef LIBTORRENT_TRACKER_TRACKER_DHT_H
#define LIBTORRENT_TRACKER_TRACKER_DHT_H

#include <array>
#include <memory>

#include "torrent/object.h"
#include "torrent/utils/scheduler.h"
#include "tracker/tracker_worker.h"

namespace torrent {

namespace dht {
class DhtAnnounce;
}

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

  void                set_weak_tracker(std::weak_ptr<TrackerDht> weak_tracker);

  state_type          dht_state() const;
  void                set_dht_announce_state();

  int                 replied() const;
  int                 contacted() const;

  bool                has_peers_unsafe() const;

  void                receive_peers(AddressList&& address_list);
  void                receive_success();
  void                receive_failed(const char* msg);
  void                receive_progress(int replied, int contacted);

protected:
  friend class torrent::dht::DhtAnnounce;

  static void         add_event(std::weak_ptr<TrackerDht> weak_tracker, std::function<void (TrackerDht*)>&& event);

private:
  void                cleanup() override;

  void                update_requesting_state();

  std::weak_ptr<TrackerDht> m_weak_tracker;

  tracker::TrackerParams  m_params;

  std::atomic<state_type> m_dht_state{state_idle};

  std::atomic<int>        m_replied;
  std::atomic<int>        m_contacted;

  utils::SchedulerEntry   m_delay_clear_state;
};

inline void TrackerDht::set_weak_tracker(std::weak_ptr<TrackerDht> weak_tracker) { m_weak_tracker = std::move(weak_tracker); }

inline TrackerDht::state_type TrackerDht::dht_state() const        { return m_dht_state; }
inline int                    TrackerDht::replied() const          { return m_replied; }
inline int                    TrackerDht::contacted() const        { return m_contacted; }

} // namespace torrent

#endif
