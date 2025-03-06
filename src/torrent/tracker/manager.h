// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <torrent/tracker/wrappers.h>

namespace torrent {
class Manager;
class ThreadTracker;
}

namespace torrent::tracker {

struct TrackerListEvent {
  TrackerList*          tracker_list;
  std::function<void()> event;
};

class LIBTORRENT_EXPORT Manager {
protected:
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class torrent::Manager;
  friend class torrent::ThreadTracker;
  friend class torrent::TrackerList;

  // Main thread:

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, TrackerController* controller);
  void                     remove_controller(TrackerControllerWrapper controller);

  void                set_main_thread_signal(unsigned int idx) { m_main_thread_signal = idx; }

  void                process_events();

  // Any thread:

  void                add_event(TrackerList* tracker_list, std::function<void()> event);

private:
  unsigned int        m_main_thread_signal{~0u};

  std::mutex                         m_lock;
  std::set<TrackerControllerWrapper> m_controllers;

  // Events should not depend on the TrackerWorker still existing.
  std::mutex                         m_events_lock;
  std::vector<TrackerListEvent>      m_tracker_list_events;
};

}

#endif // LIBTORRENT_TRACKER_MANAGER_H
