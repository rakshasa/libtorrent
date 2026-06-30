#ifndef LIBTORRENT_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <vector>
#include <torrent/tracker/tracker.h>
#include <torrent/tracker/wrappers.h>

// Thread-safe manager for all trackers loaded by the client.

namespace RTORRENT_EXPORT torrent {

class TrackerWorker;

namespace tracker {

class Manager {
public:
  Manager();
  ~Manager();

protected:
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class torrent::TrackerList;
  friend class torrent::TrackerWorker;
  friend class torrent::ThreadTracker;

  // Main thread:

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, std::shared_ptr<TrackerController> controller);
  void                     remove_controller(TrackerControllerWrapper controller);

  void                send_event(tracker::Tracker& tracker, TrackerParams params, tracker::TrackerState::event_enum new_event);
  void                send_scrape(tracker::Tracker& tracker, TrackerParams params);

  // Any thread:

  void                add_event(std::weak_ptr<TrackerWorker> weak_ptr, std::weak_ptr<void> tl_keeper, std::function<void (Tracker&)>&& event);
  void                add_event_or_update(std::weak_ptr<TrackerWorker> weak_ptr, std::weak_ptr<void> tl_keeper, std::function<void (Tracker&)>&& event);

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

} // namespace torrent

#endif
