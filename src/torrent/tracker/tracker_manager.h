// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include <memory>
#include <mutex>
#include <set>
#include <torrent/common.h>
#include <torrent/hash_string.h>
#include <torrent/tracker.h>
#include <torrent/tracker/tracker_wrappers.h>

namespace torrent {

// TODO: Don't export, this should be in src/tracker?

class LIBTORRENT_EXPORT TrackerManager {
public:
  TrackerControllerWrapper add_controller(DownloadInfo* download_info, TrackerController* controller);
  void                     remove_controller(TrackerControllerWrapper controller);

private:
  std::mutex                         m_lock;
  std::set<TrackerControllerWrapper> m_controllers;
};

}

#endif // LIBTORRENT_TRACKER_TRACKER_MANAGER_H
