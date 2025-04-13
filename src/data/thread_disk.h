#ifndef LIBTORRENT_DATA_THREAD_DISK_H
#define LIBTORRENT_DATA_THREAD_DISK_H

#include "data/hash_check_queue.h"
#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent {

class LIBTORRENT_EXPORT ThreadDisk : public utils::Thread {
public:

  static void        create_thread();
  static void        destroy_thread();
  static ThreadDisk* thread_disk();

  const char*     name() const override { return "rtorrent disk"; }

  HashCheckQueue* hash_check_queue() { return &m_hash_check_queue; }

  void            init_thread() override;

private:
  ThreadDisk() = default;
  ~ThreadDisk() override;

  void            call_events() override;
  int64_t         next_timeout_usec() override;

  static ThreadDisk* m_thread_disk;

  HashCheckQueue  m_hash_check_queue;
};

inline ThreadDisk* thread_disk() {
  return ThreadDisk::thread_disk();
}

} // namespace torrent

#endif // LIBTORRENT_DATA_THREAD_DISK_H
