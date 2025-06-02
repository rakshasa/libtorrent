// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <torrent/tracker/tracker.h>
#include <torrent/tracker/wrappers.h>

namespace torrent::tracker {

struct TrackerListEvent {
  TrackerList*          tracker_list;
  std::function<void()> event;
};

class LIBTORRENT_EXPORT Manager {
public:

  Manager(utils::Thread* main_thread, utils::Thread* tracker_thread);
  ~Manager() = default;

protected:
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class torrent::TrackerList;
  friend class torrent::ThreadTracker;

  // Main thread:

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, std::shared_ptr<TrackerController> controller);
  void                     remove_controller(TrackerControllerWrapper controller);

  void                send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event);
  void                send_scrape(tracker::Tracker& tracker);

  // Any thread:

  // remove_events() only removes events from the main thread.
  void                add_event(torrent::TrackerWorker* tracker_worker, std::function<void()> event);
  void                remove_events(torrent::TrackerWorker* tracker_worker);

private:
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  utils::Thread*      m_main_thread{nullptr};
  utils::Thread*      m_tracker_thread{nullptr};
  unsigned int        m_signal_process_events{~0u};

  std::mutex                         m_lock;
  std::set<TrackerControllerWrapper> m_controllers;
};

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_MANAGER_H
