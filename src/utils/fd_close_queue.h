#ifndef LIBTORRENT_UTILS_FD_CLOSE_QUEUE_H
#define LIBTORRENT_UTILS_FD_CLOSE_QUEUE_H

#include <deque>
#include <future>
#include <torrent/common.h>

namespace torrent::utils {

// Use a persistent std::async thread started first use, to close file descriptors on a background thread. Use std::atomic to unblock the worker thread.
//
// We also have a flush_and_wait function, and a way to manually close a file descriptor to free up fds. (implement later, we use a unique counter to indicate... do this later)

class FdCloseQueue {
public:
  FdCloseQueue();
  ~FdCloseQueue();

  uint32_t            size() const;

  void                close_fd(int fd);
  void                close_fds(std::vector<int>&& fds);

  void                wait_for(uint32_t max_remaining);

private:
  FdCloseQueue(const FdCloseQueue&) = delete;
  FdCloseQueue& operator=(const FdCloseQueue&) = delete;

  std::future<void>   m_worker;

  align_cacheline

  std::mutex          m_mutex;
  std::vector<int>    m_queue;

  bool                m_should_shutdown{};

  align_cacheline

  std::atomic<bool>     m_wakeup_worker{};
  std::atomic<uint32_t> m_remaining{};
};

inline uint32_t FdCloseQueue::size() const { return m_remaining.load(std::memory_order_acquire); }

} // namespace torrent::utils

#endif
