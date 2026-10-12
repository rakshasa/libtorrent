#include "config.h"

#include "utils/memory_unmap_queue.h"

#include <sys/mman.h>

#include "torrent/exceptions.h"

namespace torrent::utils {

MemoryUnmapQueue::MemoryUnmapQueue() {
  m_worker = std::async(std::launch::async, [this]() {
      bool is_running = true;

      while (is_running) {
        m_wakeup_worker.wait(false, std::memory_order_acquire);

        queue_type queue;

        {
          std::lock_guard<std::mutex> guard(m_mutex);

          if (m_should_shutdown) {
            if (m_queue.empty())
              return;

            is_running = false;
          }

          if (m_queue.empty())
            throw internal_error("MemoryUnmapQueue worker thread woke up but queue is empty.");

          queue.swap(m_queue);

          m_wakeup_worker.store(false, std::memory_order_release);
        }

        // MS_ASYNC starts writeback of dirty MAP_SHARED pages as regions are
        // retired so the kernel does not accumulate multi-GB dirty sets.
        // Failures are ignored — munmap still proceeds; durability is not
        // required here.
        for (auto& region : queue) {
          ::msync(region.first, region.second, MS_ASYNC);
          ::munmap(region.first, region.second);
        }
      }
    });
}

MemoryUnmapQueue::~MemoryUnmapQueue() {
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_should_shutdown = true;
  }

  m_wakeup_worker.store(true, std::memory_order_release);
  m_wakeup_worker.notify_all();

  m_worker.wait();
}

void
MemoryUnmapQueue::queue(void* ptr, size_t length) {
  if (ptr == nullptr || length == 0)
    return;

  {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_queue.empty()) {
      m_queue.emplace_back(ptr, length);
      return;
    }

    m_queue.emplace_back(ptr, length);
  }

  m_wakeup_worker.store(true, std::memory_order_release);
  m_wakeup_worker.notify_all();
}

} // namespace torrent::utils
