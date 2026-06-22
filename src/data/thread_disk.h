#ifndef LIBTORRENT_DATA_THREAD_DISK_H
#define LIBTORRENT_DATA_THREAD_DISK_H

#include <memory>

#include "torrent/common.h"
#include "torrent/system/thread.h"

namespace torrent {

class HashCheckQueue;

class LIBTORRENT_EXPORT ThreadDisk : public system::Thread {
public:
  ~ThreadDisk() override;

  static void        create_thread();
  static void        destroy_thread();
  static ThreadDisk* thread_disk();

  const char*        name() const override { return "rtorrent-disk"; }

  void               init_thread() override;
  void               cleanup_thread() override;

  HashCheckQueue*    hash_check_queue()    { return m_hash_check_queue.get(); }

private:
  ThreadDisk() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  static ThreadDisk*              m_thread_disk;

  std::unique_ptr<HashCheckQueue> m_hash_check_queue;
};

} // namespace torrent

#endif // LIBTORRENT_DATA_THREAD_DISK_H
