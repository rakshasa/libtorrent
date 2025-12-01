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

struct TrackerInfo {
  HashString  info_hash{HashString::new_zero()};
  HashString  obfuscated_hash{HashString::new_zero()};
  HashString  local_id{HashString::new_zero()};

  std::string url;
  uint32_t    key{0};
};

struct TrackerParameters {
  int32_t  numwant{-1};
  uint64_t uploaded_adjusted{0};
  uint64_t completed_adjusted{0};
  uint64_t download_left{0};
};

class TrackerWorker {
public:
  TrackerWorker(TrackerInfo info, int flags = 0);
  virtual ~TrackerWorker();

  // Public members do not require locking:

  const TrackerInfo&   info() const { return m_info; }
  virtual tracker_enum type() const = 0;

  virtual std::string  lock_and_status() const { return ""; }

  virtual void        send_event(tracker::TrackerState::event_enum state) = 0;
  virtual void        send_scrape() = 0;

protected:
  friend class TrackerList;
  friend class tracker::Tracker;
  friend class ::TrackerTest;

  void                lock_and_clear_intervals();
  void                lock_and_clear_stats();
  void                lock_and_set_latest_event(tracker::TrackerState::event_enum new_state);

  void                lock() const                          { m_mutex.lock(); }
  auto                lock_guard() const                    { return std::scoped_lock(m_mutex); }
  void                unlock() const                        { m_mutex.unlock(); }

  // Protected members that require locking:

  virtual bool        is_busy() const = 0;
  virtual bool        is_usable() const                     { return m_state.is_enabled(); }

  bool                is_busy_not_scrape() const;

  std::string         tracker_id() const                    { return m_tracker_id; }
  void                set_tracker_id(const std::string& id) { m_tracker_id = id; }

  uint32_t            group() const                         { return m_group; }
  void                set_group(uint32_t v)                 { m_group = v; }

  tracker::TrackerState&       state()                      { return m_state; }
  const tracker::TrackerState& state() const                { return m_state; }

  // Do not lock when calling these functions/slots:
  //
  // TODO: Review, these should be called locked? Yes.
  //
  // The slots should put a work order into the tracker controller thread, which will be correctly
  // ordered as it pertains to a single tracker's slot calls.

  virtual void        close() = 0;

  std::function<void()>              m_slot_enabled;
  std::function<void()>              m_slot_disabled;
  std::function<void()>              m_slot_close;
  std::function<void(AddressList&&)> m_slot_success;
  std::function<void(std::string)>   m_slot_failure;
  std::function<void()>              m_slot_scrape_success;
  std::function<void(std::string)>   m_slot_scrape_failure;
  std::function<void(AddressList&&)> m_slot_new_peers;
  std::function<TrackerParameters()> m_slot_parameters;

private:
  TrackerWorker(const TrackerWorker&) = delete;
  TrackerWorker& operator=(const TrackerWorker&) = delete;

  mutable std::mutex    m_mutex;

  TrackerInfo           m_info;

  tracker::TrackerState m_state{};
  std::string           m_tracker_id;
  uint32_t              m_group{0};
};

inline TrackerWorker::TrackerWorker(TrackerInfo info, int flags)
  : m_info(std::move(info)) {
  m_state.m_flags = flags;
}

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

inline void
TrackerWorker::lock_and_set_latest_event(tracker::TrackerState::event_enum new_state) {
  auto guard = lock_guard();
  m_state.m_latest_event = new_state;
}

inline bool
TrackerWorker::is_busy_not_scrape() const {
  return is_busy() && state().latest_event() != tracker::TrackerState::EVENT_SCRAPE;
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WORKER_H
