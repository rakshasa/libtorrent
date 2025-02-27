// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include <mutex>
#include <set>
#include <torrent/tracker/wrappers.h>

namespace torrent::tracker {

class LIBTORRENT_EXPORT Manager {
public:
  // TODO: Make protected.

  TrackerControllerWrapper add_controller(DownloadInfo* download_info, TrackerController* controller);
  void                     remove_controller(TrackerControllerWrapper controller);

private:
  std::mutex                         m_lock;
  std::set<TrackerControllerWrapper> m_controllers;
};

}

#endif // LIBTORRENT_TRACKER_TRACKER_MANAGER_H
