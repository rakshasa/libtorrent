#ifndef LIBTORRENT_THREAD_TRACKER_H
#define LIBTORRENT_THREAD_TRACKER_H

#include "torrent/tracker/tracker.h"
#include "torrent/utils/thread.h"

namespace torrent {

namespace tracker {
class Manager;
}

struct TrackerSendEvent {
  tracker::Tracker                  tracker;
  tracker::TrackerState::event_enum event;
};

class ThreadTracker : public utils::Thread {
public:
  ThreadTracker(utils::Thread* main_thread);
  ~ThreadTracker();

  const char*         name() const      { return "rtorrent tracker"; }

  virtual void        init_thread();

  tracker::Manager*   tracker_manager() { return m_tracker_manager.get(); }

  void                send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event);

protected:
  friend class Manager;

  virtual void        call_events();
  virtual int64_t     next_timeout_usec();

private:
  void                process_send_events();

  std::unique_ptr<tracker::Manager> m_tracker_manager;

  unsigned int                  m_signal_send_event{~0u};

  std::mutex                    m_send_events_lock;
  std::vector<TrackerSendEvent> m_send_events;
};

} // namespace torrent

#endif // LIBTORRENT_THREAD_TRACKER_H
