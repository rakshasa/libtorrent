// TrackerWorker is the base class for trackers and should be thread-safe.

#ifndef LIBTORRENT_TRACKER_TRACKER_WORKER_H
#define LIBTORRENT_TRACKER_TRACKER_WORKER_H

#include <iosfwd>
#include <functional>
#include <string>

#include "torrent/object.h"
#include "torrent/tracker.h"

class TrackerTest;

namespace torrent {

class Tracker;
class TrackerList;

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
  static const int flag_enabled       = 0x1;
  static const int flag_extra_tracker = 0x2;
  static const int flag_can_scrape    = 0x4;

  static const int max_flag_size   = 0x10;
  static const int mask_base_flags = 0x10 - 1;

  TrackerWorker(const TrackerInfo& info, int flags = 0);
  virtual ~TrackerWorker() = default;

  TrackerWorker() = delete;
  TrackerWorker(const TrackerWorker&) = delete;
  TrackerWorker& operator=(const TrackerWorker&) = delete;

  const TrackerInfo&    info() const { return m_info; }
  tracker::TrackerState state() const;

protected:
  friend class Tracker;
  friend class TrackerList;
  friend class ::TrackerTest;

  // All protected methods are non-thread safe and require a lock.
  void                lock() const              { m_state_mutex.lock(); }
  auto                lock_guard() const        { return std::lock_guard<std::mutex>(m_state_mutex); }
  void                unlock() const            { m_state_mutex.unlock(); }

  int                 flags() const             { return m_flags; }

  bool                is_enabled() const        { return (m_flags & flag_enabled); }
  bool                is_extra_tracker() const  { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const         { return is_enabled() && state().success_counter() != 0; }
  virtual bool        is_usable() const         { return is_enabled(); }

  virtual bool        is_busy() const = 0;
  bool                is_busy_not_scrape() const { return state().latest_event() != tracker::TrackerState::EVENT_SCRAPE && is_busy(); }

  bool                can_scrape() const         { return (m_flags & flag_can_scrape); }

  std::string          tracker_id() const;
  virtual tracker_enum type() const = 0;

  bool                enable();
  bool                disable();

  virtual void        send_event(tracker::TrackerState::event_enum state) = 0;
  virtual void        send_scrape() = 0;
  virtual void        close() = 0;
  virtual void        disown() = 0;

  // TODO: Make thread safe:
  void                set_latest_event(tracker::TrackerState::event_enum event);
  void                set_state(const tracker::TrackerState& state);

  void                update_tracker_id(const std::string& id);

  // Only the tracker thread should call these.
  void                clear_intervals();
  void                clear_stats();

  // TODO: Move to private.
  int                 m_flags;

  std::function<void(AddressList&&)> m_slot_success;
  std::function<void(std::string)>   m_slot_failure;
  std::function<void()>              m_slot_scrape_success;
  std::function<void(std::string)>   m_slot_scrape_failure;
  std::function<TrackerParameters()> m_slot_parameters;

private:
  TrackerInfo           m_info;

  // Only the tracker thread should change state.
  mutable std::mutex    m_state_mutex;
  tracker::TrackerState m_state;

  std::string         m_tracker_id;
};


inline std::string
TrackerWorker::tracker_id() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_tracker_id;
}

inline tracker::TrackerState
TrackerWorker::state() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_state;
}

// TODO: Use lambda function to update within the lock.
inline void
TrackerWorker::set_state(const tracker::TrackerState& state) {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  m_state = state;
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WORKER_H
