#ifndef LIBTORRENT_THREAD_TRACKER_H
#define LIBTORRENT_THREAD_TRACKER_H

#include <vector>

#include "torrent/common.h"
#include "torrent/tracker/tracker.h"
#include "torrent/system/thread.h"

namespace torrent {

namespace tracker {
class Manager;
} // namespace tracker

struct TrackerSendEvent {
  tracker::Tracker                  tracker;
  tracker::TrackerState::event_enum event;
};

class LIBTORRENT_EXPORT ThreadTracker : public system::Thread {
public:
  ~ThreadTracker() override;

  static void           create_thread(system::Thread* main_thread);
  static void           destroy_thread();
  static ThreadTracker* thread_tracker();

  const char*           name() const override { return "rtorrent tracker"; }

  void                  init_thread() override;
  void                  cleanup_thread() override;

  tracker::Manager*     tracker_manager() { return m_tracker_manager.get(); }

protected:
  friend class Manager;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

private:
  ThreadTracker() = default;

  static ThreadTracker*              m_thread_tracker;

  std::unique_ptr<tracker::Manager>  m_tracker_manager;
  unsigned int                       m_signal_send_event{~0u};
};

} // namespace torrent

#endif // LIBTORRENT_THREAD_TRACKER_H
