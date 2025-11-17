#ifndef LIBTORRENT_DATA_THREAD_DISK_H
#define LIBTORRENT_DATA_THREAD_DISK_H

#include "data/hash_check_queue.h"
#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent {

class LIBTORRENT_EXPORT ThreadDisk : public utils::Thread {
public:
  ~ThreadDisk() override;

  static void        create_thread();
  static void        destroy_thread();
  static ThreadDisk* thread_disk();

  const char*     name() const override { return "rtorrent disk"; }

  HashCheckQueue* hash_check_queue() { return &m_hash_check_queue; }

  void            init_thread() override;
  void            cleanup_thread() override;

private:
  ThreadDisk() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  static ThreadDisk* m_thread_disk;

  HashCheckQueue  m_hash_check_queue;
};

inline ThreadDisk* thread_disk() {
  return ThreadDisk::thread_disk();
}

} // namespace torrent

#endif // LIBTORRENT_DATA_THREAD_DISK_H
