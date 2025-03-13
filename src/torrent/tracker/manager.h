// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <torrent/tracker/tracker.h>
#include <torrent/tracker/wrappers.h>

namespace torrent {
class Manager;
// class ThreadTracker;
}

namespace torrent::tracker {

struct TrackerListEvent {
  TrackerList*          tracker_list;
  std::function<void()> event;
};

class LIBTORRENT_EXPORT Manager {
public:

  Manager();

protected:
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class torrent::Manager;
  // friend class torrent::ThreadTracker;
  friend class torrent::TrackerList;

  // Main thread:

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, TrackerController* controller);
  void                     remove_controller(TrackerControllerWrapper controller);

  void                send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event);

  // Any thread:

  void                add_event(TrackerList* tracker_list, std::function<void()> event);

private:
  void                process_events();

  unsigned int        m_signal_process_events{~0u};

  std::mutex                         m_lock;
  std::set<TrackerControllerWrapper> m_controllers;

  // Events should not depend on the TrackerWorker still existing.
  std::mutex                         m_events_lock;
  std::vector<TrackerListEvent>      m_tracker_list_events;
};

}

#endif // LIBTORRENT_TRACKER_MANAGER_H
