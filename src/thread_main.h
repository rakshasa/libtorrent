#ifndef LIBTORRENT_THREAD_MAIN_H
#define LIBTORRENT_THREAD_MAIN_H

#include "data/hash_check_queue.h"
#include "torrent/utils/thread.h"

namespace torrent {

class ThreadMain : public ThreadBase {
public:
  const char*         name() const { return "rtorrent main"; }

  virtual void        init_thread();

protected:
  virtual void        call_events();
  virtual int64_t     next_timeout_usec();
};

}

#endif
