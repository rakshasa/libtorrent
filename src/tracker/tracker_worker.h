// TrackerWorker is the base class for trackers and should be thread-safe.

#ifndef LIBTORRENT_TRACKER_TRACKER_WORKER_H
#define LIBTORRENT_TRACKER_TRACKER_WORKER_H

#include <functional>
#include <iosfwd>
#include <mutex>
#include <utility>

#include "torrent/common.h"
#include "torrent/hash_string.h"
#include "torrent/tracker/tracker_state.h"

class TrackerTest;

namespace torrent {

namespace tracker {
class Manager;
}

struct TrackerInfo {
  HashString  info_hash{HashString::new_zero()};
  HashString  obfuscated_hash{HashString::new_zero()};
  HashString  local_id{HashString::new_zero()};

  std::string url;
  uint32_t    group{};
  uint32_t    key{};
};

class TrackerWorker {
public:
  TrackerWorker(TrackerInfo info, int flags = 0);
  virtual ~TrackerWorker() noexcept(false);

  // Public members do not require locking:

  const TrackerInfo&   info() const { return m_info; }

  virtual tracker_enum type() const = 0;

  virtual void         send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum state) = 0;
  virtual void         send_scrape(tracker::TrackerParams params) = 0;

protected:
  friend class TrackerList;
  friend class tracker::Tracker;
  friend class tracker::Manager;
  friend class ::TrackerTest;

  virtual void        close() = 0;
  virtual void        cleanup() = 0;

  void                lock_and_clear_intervals();
  void                lock_and_clear_stats();
  auto                lock_and_latest_event();
  void                lock_and_set_latest_event(tracker::TrackerState::event_enum new_state);

  void                lock() const                          { m_mutex.lock(); }
  auto                lock_guard() const                    { return std::scoped_lock(m_mutex); }
  void                unlock() const                        { m_mutex.unlock(); }

  // Protected members that require locking:

  std::string         tracker_id_safe() const;
  void                set_tracker_id_safe(const std::string& id);

  auto*               callback_ptr()                        { return &m_callback; }
  void                remove_events();

  tracker::TrackerState&       state()                      { return m_state; }
  const tracker::TrackerState& state() const                { return m_state; }

  // Do not lock when calling these functions/slots:
  //
  // TODO: Review, these should be called locked? Yes.
  //
  // The slots should put a work order into the tracker controller thread, which will be correctly
  // ordered as it pertains to a single tracker's slot calls.

  std::function<void()>              m_slot_enabled;
  std::function<void()>              m_slot_disabled;
  std::function<void(AddressList&&)> m_slot_success;
  std::function<void(std::string)>   m_slot_failure;
  std::function<void()>              m_slot_scrape_success;
  std::function<void(std::string)>   m_slot_scrape_failure;
  std::function<void(AddressList&&)> m_slot_new_peers;

private:
  TrackerWorker(const TrackerWorker&) = delete;
  TrackerWorker& operator=(const TrackerWorker&) = delete;

  mutable std::mutex    m_mutex;
  std::atomic<uint32_t> m_callback{};

  TrackerInfo           m_info;

  tracker::TrackerState m_state{};
  std::string           m_tracker_id;

  std::mutex            __force_new_cacheline;
};

inline void
TrackerWorker::lock_and_clear_intervals() {
  auto guard = lock_guard();
  m_state.clear_intervals();
}

inline void
TrackerWorker::lock_and_clear_stats() {
  auto guard = lock_guard();
  m_state.clear_stats();
}

inline auto
TrackerWorker::lock_and_latest_event() {
  auto guard = lock_guard();
  return m_state.m_latest_event;
}

inline void
TrackerWorker::lock_and_set_latest_event(tracker::TrackerState::event_enum new_state) {
  auto guard = lock_guard();
  m_state.m_latest_event = new_state;
}

inline std::string
TrackerWorker::tracker_id_safe() const {
  auto guard = lock_guard();
  return m_tracker_id;
}

inline void
TrackerWorker::set_tracker_id_safe(const std::string& id) {
  auto guard = lock_guard();
  m_tracker_id = id;
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WORKER_H
