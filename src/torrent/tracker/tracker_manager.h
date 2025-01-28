// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include <memory>
#include <mutex>
#include <set>
#include <torrent/common.h>

namespace torrent {

class Tracker;
class TrackerManager;

// TODO: This should be renamed to Tracker later.
class LIBTORRENT_EXPORT TrackerWrapper {
public:
  TrackerWrapper(std::shared_ptr<Tracker>&& tracker) : m_tracker(tracker) {}

protected:
  friend class TrackerManager;

  Tracker* get() const { return m_tracker.get(); }

private:
  std::shared_ptr<Tracker> m_tracker;
};

class LIBTORRENT_EXPORT TrackerManager : private std::set<std::shared_ptr<Tracker>> {
public:
  TrackerWrapper add(Tracker* tracker);
  void           remove(TrackerWrapper tracker);

private:
  std::mutex     m_lock;
};

}

#endif // LIBTORRENT_TRACKER_TRACKER_MANAGER_H
