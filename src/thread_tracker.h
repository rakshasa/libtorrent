#ifndef LIBTORRENT_THREAD_TRACKER_H
#define LIBTORRENT_THREAD_TRACKER_H

#include "data/hash_check_queue.h"
#include "torrent/utils/thread_base.h"

namespace torrent {

namespace tracker {
class Manager;
}

class ThreadTracker : public thread_base {
public:
  ThreadTracker(tracker::Manager* manager) : m_manager(manager) {}

  const char*     name() const { return "rtorrent tracker"; }

  virtual void    init_thread();

protected:
  virtual void    call_events();
  virtual int64_t next_timeout_usec();

  tracker::Manager* m_manager;
};

}

#endif
