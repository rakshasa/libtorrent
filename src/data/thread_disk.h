#ifndef LIBTORRENT_DATA_THREAD_DISK_H
#define LIBTORRENT_DATA_THREAD_DISK_H

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

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

  // Detach on main first, then queue the raw fd here. Disk thread owns ::close.
  void               queue_close_fd(int fd);
  void               queue_close_fds(const std::vector<int>& fds);

private:
  ThreadDisk() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  void               perform_close_fds();

  static ThreadDisk*              m_thread_disk;

  std::unique_ptr<HashCheckQueue> m_hash_check_queue;

  std::mutex                      m_close_fds_lock;
  std::deque<int>                 m_close_fds;
};

} // namespace torrent

#endif // LIBTORRENT_DATA_THREAD_DISK_H
