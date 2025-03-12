#ifndef LIBTORRENT_THREAD_DISK_H
#define LIBTORRENT_THREAD_DISK_H

#include "data/hash_check_queue.h"
#include "torrent/utils/thread.h"

namespace torrent {

class ThreadDisk : public ThreadBase {
public:
  ThreadDisk() = default;
  ~ThreadDisk() override = default;

  const char*     name() const override { return "rtorrent disk"; }
  HashCheckQueue* hash_check_queue() { return &m_hash_check_queue; }

  void            init_thread() override;

protected:
  void            call_events() override;
  int64_t         next_timeout_usec() override;

  HashCheckQueue  m_hash_check_queue;
};

extern ThreadDisk* thread_disk;

}

#endif
