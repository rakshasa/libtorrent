#ifndef LIBTORRENT_UTILS_MEMORY_UNMAP_QUEUE_H
#define LIBTORRENT_UTILS_MEMORY_UNMAP_QUEUE_H

#include <future>
#include <mutex>
#include <utility>
#include <vector>
#include <torrent/common.h>

namespace torrent::utils {

// Persistent std::async worker that issues MS_ASYNC + munmap off the caller
// (and off ThreadDisk). Same shape as FdCloseQueue.

class MemoryUnmapQueue {
public:
  MemoryUnmapQueue();
  ~MemoryUnmapQueue();

  void                queue(void* ptr, size_t length);

private:
  MemoryUnmapQueue(const MemoryUnmapQueue&) = delete;
  MemoryUnmapQueue& operator=(const MemoryUnmapQueue&) = delete;

  using queue_type = std::vector<std::pair<void*, size_t>>;

  std::future<void>   m_worker;

  align_cacheline

  std::mutex          m_mutex;
  queue_type          m_queue;

  bool                m_should_shutdown{};

  align_cacheline

  std::atomic<bool>   m_wakeup_worker{};
};

} // namespace torrent::utils

#endif
