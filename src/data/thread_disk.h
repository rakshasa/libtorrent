#ifndef LIBTORRENT_THREAD_DISK_H
#define LIBTORRENT_THREAD_DISK_H

#include "data/hash_check_queue.h"
#include "torrent/utils/thread_base.h"

namespace torrent {

class ThreadDisk : public thread_base {
public:
  ThreadDisk() = default;
  ~ThreadDisk() override = default;

  const char*     name() const override { return "rtorrent disk"; }
  HashCheckQueue* hash_queue() { return &m_hash_queue; }

  void            init_thread() override;

protected:
  void            call_events() override;
  int64_t         next_timeout_usec() override;

  HashCheckQueue  m_hash_queue;
};

extern ThreadDisk* thread_disk;

}

#endif
