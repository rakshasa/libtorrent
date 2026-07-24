#include "config.h"

#include "utils/fd_close_queue.h"

#include <unistd.h>

#include "torrent/exceptions.h"

namespace torrent::utils {

FdCloseQueue::FdCloseQueue() {
  m_worker = std::async(std::launch::async, [this]() {
      bool is_running = true;

      while (is_running) {
        m_wakeup_worker.wait(false, std::memory_order_acquire);

        std::vector<int> queue;

        {
          std::lock_guard<std::mutex> guard(m_mutex);

          if (m_should_shutdown) {
            if (m_queue.empty())
              return;

            is_running = false;
          }

          if (m_queue.empty())
            throw internal_error("FdCloseQueue worker thread woke up but queue is empty.");

          queue.swap(m_queue);

          m_wakeup_worker.store(false, std::memory_order_release);
        }

        for (int fd : queue) {
          ::close(fd);

          m_remaining.fetch_sub(1, std::memory_order_release);
          m_remaining.notify_all();
        }
      }
    });
}

FdCloseQueue::~FdCloseQueue() {
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_should_shutdown = true;
  }

  m_wakeup_worker.store(true, std::memory_order_release);
  m_wakeup_worker.notify_all();

  m_worker.wait();
}

void
FdCloseQueue::close_fd(int fd) {
  if (fd < 0)
    throw internal_error("FdCloseQueue::close_fd() called with invalid fd.");

  m_remaining.fetch_add(1, std::memory_order_acquire);

  {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_queue.empty()) {
      m_queue.push_back(fd);
      return;
    }

    m_queue.push_back(fd);
  }

  m_wakeup_worker.store(true, std::memory_order_release);
  m_wakeup_worker.notify_all();
}

void
FdCloseQueue::close_fds(std::vector<int>&& fds) {
  if (fds.empty())
    return;

  m_remaining.fetch_add(fds.size(), std::memory_order_acquire);

  {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (!m_queue.empty()) {
      m_queue.insert(m_queue.end(), fds.begin(), fds.end());
      return;
    }

    m_queue.swap(fds);
  }

  m_wakeup_worker.store(true, std::memory_order_release);
  m_wakeup_worker.notify_all();
}

void
FdCloseQueue::wait_for(uint32_t max_remaining) {
  while (m_remaining.load(std::memory_order_acquire) > max_remaining)
    m_remaining.wait(max_remaining, std::memory_order_acquire);
}

} // namespace torrent::utils
