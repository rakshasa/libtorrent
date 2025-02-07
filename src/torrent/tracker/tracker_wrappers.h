// Temporary classes for thread-safe tracker related classes.
//
// These will be renamed after movimg the old torrent/tracker_* classes to src/tracker.

#ifndef LIBTORRENT_TRACKER_TRACKER_WRAPPER_H
#define LIBTORRENT_TRACKER_TRACKER_WRAPPER_H

#include <memory>
#include <torrent/common.h>
#include <torrent/hash_string.h>

namespace torrent {

// TODO: This should be renamed to Tracker later.
class LIBTORRENT_EXPORT TrackerWrapper {
public:
  TrackerWrapper(const HashString& info_hash, std::shared_ptr<Tracker>&& tracker);

  bool operator<(const TrackerWrapper& rhs) const;

protected:
  friend class TrackerManager;

  Tracker*                  get()        { return m_ptr.get(); }
  const Tracker*            get() const  { return m_ptr.get(); }
  std::shared_ptr<Tracker>& get_shared() { return m_ptr; }

  const HashString&         info_hash() const { return m_info_hash; }

private:
  std::shared_ptr<Tracker> m_ptr;
  HashString               m_info_hash;
};

inline
TrackerWrapper::TrackerWrapper(const HashString& info_hash, std::shared_ptr<Tracker>&& tracker) :
  m_ptr(tracker),
  m_info_hash(info_hash) {
}

inline bool
TrackerWrapper::operator<(const TrackerWrapper& rhs) const {
  return this->get() < rhs.get();
}

class LIBTORRENT_EXPORT TrackerControllerWrapper {
public:
  TrackerControllerWrapper(const HashString& info_hash, std::shared_ptr<TrackerController>&& controller);

  bool operator<(const TrackerControllerWrapper& rhs) const;

protected:
  friend class TrackerManager;

  TrackerController*                  get()        { return m_ptr.get(); }
  const TrackerController*            get() const  { return m_ptr.get(); }
  std::shared_ptr<TrackerController>& get_shared() { return m_ptr; }

  const HashString&                   info_hash() const { return m_info_hash; }

private:
  std::shared_ptr<TrackerController> m_ptr;
  HashString                         m_info_hash;
};

inline
TrackerControllerWrapper::TrackerControllerWrapper(const HashString& info_hash, std::shared_ptr<TrackerController>&& controller) :
  m_ptr(controller),
  m_info_hash(info_hash) {
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WRAPPER_H
