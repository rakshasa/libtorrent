// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <vector>
#include <torrent/tracker/tracker.h>
#include <torrent/tracker/wrappers.h>

namespace torrent::tracker {

class LIBTORRENT_EXPORT Manager {
public:
  Manager();
  ~Manager();

protected:
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class torrent::TrackerList;
  friend class torrent::ThreadTracker;

  // Main thread:

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, std::shared_ptr<TrackerController> controller);
  void                     remove_controller(TrackerControllerWrapper controller);

  void                send_event(tracker::Tracker& tracker, TrackerParams params, tracker::TrackerState::event_enum new_event);
  void                send_scrape(tracker::Tracker& tracker, TrackerParams params);

  // Any thread:

  void                add_event(TrackerWorker* worker, std::function<void ()>&& event);
  void                remove_events(TrackerWorker* worker);

  void                update_tracker(const Tracker& tracker);

  void                delete_tracker(Tracker tracker);
  void                delete_trackers(std::vector<Tracker>&& trackers);

private:
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  void                process_delete_trackers();

  std::mutex          m_lock;

  std::set<TrackerControllerWrapper> m_controllers;
  std::vector<Tracker>               m_trackers_to_wait;
  std::vector<Tracker>               m_trackers_to_delete;
};

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_MANAGER_H
