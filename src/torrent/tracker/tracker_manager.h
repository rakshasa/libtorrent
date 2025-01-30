// Thread-safe manager for all trackers loaded by the client.

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include <memory>
#include <mutex>
#include <set>
#include <torrent/common.h>
#include <torrent/hash_string.h>
#include <torrent/tracker.h>

namespace torrent {

class DownloadInfo;
class TrackerManager;

// TODO: This should be renamed to Tracker later.
class LIBTORRENT_EXPORT TrackerWrapper {
public:
  TrackerWrapper(const HashString& info_hash, std::shared_ptr<Tracker>&& tracker);

  bool operator<(const TrackerWrapper& rhs) const;

protected:
  friend class TrackerManager;

  Tracker*                  get()        { return m_tracker.get(); }
  const Tracker*            get() const  { return m_tracker.get(); }
  std::shared_ptr<Tracker>& get_shared() { return m_tracker; }

  const HashString&         info_hash() const { return m_info_hash; }

private:
  std::shared_ptr<Tracker> m_tracker;
  HashString               m_info_hash;
};

inline
TrackerWrapper::TrackerWrapper(const HashString& info_hash, std::shared_ptr<Tracker>&& tracker) :
  m_tracker(tracker),
  m_info_hash(info_hash) {
}

inline bool
TrackerWrapper::operator<(const TrackerWrapper& rhs) const {
  return this->get() < rhs.get();
}

class LIBTORRENT_EXPORT TrackerManager : private std::set<TrackerWrapper> {
public:
  typedef std::set<TrackerWrapper> base_type;

  TrackerWrapper add(DownloadInfo* download_info, Tracker* tracker_worker);
  void           remove(TrackerWrapper tracker);

private:
  std::mutex     m_lock;
};

}

#endif // LIBTORRENT_TRACKER_TRACKER_MANAGER_H
