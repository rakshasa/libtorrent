// TrackerWorker is the base class for trackers and should be thread-safe.

#ifndef LIBTORRENT_TRACKER_TRACKER_WORKER_H
#define LIBTORRENT_TRACKER_TRACKER_WORKER_H

#include <iosfwd>
#include <functional>
#include <string>

#include "torrent/object.h"
#include "torrent/tracker.h"

namespace torrent {

class TrackerList;

class TrackerWorker {
public:
  static const int flag_enabled       = 0x1;
  static const int flag_extra_tracker = 0x2;
  static const int flag_can_scrape    = 0x4;

  static const int max_flag_size   = 0x10;
  static const int mask_base_flags = 0x10 - 1;

  TrackerWorker(TrackerList* parent, const std::string& url, int flags = 0);
  virtual ~TrackerWorker() = default;

  TrackerWorker() = delete;
  TrackerWorker(const TrackerWorker&) = delete;
  TrackerWorker& operator=(const TrackerWorker&) = delete;

  // TODO: Make thread safe:
  int                 flags() const { return m_flags; }

  bool                is_enabled() const        { return (m_flags & flag_enabled); }
  bool                is_extra_tracker() const  { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const         { return is_enabled() && state().success_counter() != 0; }
  virtual bool        is_usable() const         { return is_enabled(); }

  virtual bool        is_busy() const = 0;
  bool                is_busy_not_scrape() const { return state().latest_event() != TrackerState::EVENT_SCRAPE && is_busy(); }

  bool                can_scrape() const        { return (m_flags & flag_can_scrape); }

  const std::string&   url() const               { return m_url; }
  std::string          tracker_id() const;
  virtual tracker_enum type() const = 0;

  TrackerState        state() const;;

  bool                enable();
  bool                disable();

  virtual void        send_event(TrackerState::event_enum state) = 0;
  virtual void        send_scrape() = 0;
  virtual void        close() = 0;
  virtual void        disown() = 0;

protected:
  friend class TrackerList;

  // TODO: Make thread safe:
  void                set_latest_event(TrackerState::event_enum event);
  void                set_state(const TrackerState& state);
  void                update_tracker_id(const std::string& id);

  // Only the tracker thread should call these.
  void                clear_intervals();
  void                clear_stats();

  // TODO: Move to private.
  int                 m_flags;

  // TODO: Remove.
  TrackerList*        m_parent;

  // TODO: Connect.
  std::function<void(AddressList&&)> m_slot_success;
  std::function<void(std::string)>   m_slot_failure;
  std::function<void()>              m_slot_scrape_success;
  std::function<void(std::string)>   m_slot_scrape_failure;

private:
  // Only the tracker thread should change state.
  mutable std::mutex  m_state_mutex;
  TrackerState        m_state;

  std::string         m_url;
  std::string         m_tracker_id;
};


inline std::string
TrackerWorker::tracker_id() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_tracker_id;
}

inline TrackerState
TrackerWorker::state() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_state;
}

// TODO: Use lambda function to update within the lock.
inline void
TrackerWorker::set_state(const TrackerState& state) {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  m_state = state;
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WORKER_H
