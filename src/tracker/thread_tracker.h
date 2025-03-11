#ifndef LIBTORRENT_THREAD_TRACKER_H
#define LIBTORRENT_THREAD_TRACKER_H

#include "torrent/utils/thread_base.h"

namespace torrent {

namespace tracker {
class Manager;
}

class ThreadTracker : public thread_base {
public:
  ThreadTracker();

  const char*         name() const      { return "rtorrent tracker"; }

  virtual void        init_thread();

  tracker::Manager*   tracker_manager() { return m_tracker_manager.get(); }

protected:
  friend class Manager;

  virtual void        call_events();
  virtual int64_t     next_timeout_usec();

  std::unique_ptr<tracker::Manager>  m_tracker_manager;
};

extern ThreadTracker* thread_tracker;

}

#endif
