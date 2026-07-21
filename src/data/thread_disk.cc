#include "config.h"

#include "data/thread_disk.h"

#include <cassert>
#include <unistd.h>

#include "thread_main.h"
#include "data/hash_check_queue.h"
#include "data/hash_queue.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

namespace torrent {

namespace disk_thread {

system::Thread* thread()                                                                 { return ThreadDisk::thread_disk(); }
std::thread::id thread_id()                                                              { return ThreadDisk::thread_disk()->thread_id(); }

void            callback(std::function<void ()>&& fn)                                    { ThreadDisk::thread_disk()->callback(std::move(fn)); }
void            callback(system::callback_id& id, std::function<void ()>&& fn)           { ThreadDisk::thread_disk()->callback(id, std::move(fn)); }
void            callback_interrupt(std::function<void ()>&& fn)                          { ThreadDisk::thread_disk()->callback_interrupt(std::move(fn)); }
void            callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) { ThreadDisk::thread_disk()->callback_interrupt(id, std::move(fn)); }

void            cancel_callback(system::callback_id& id)                                 { ThreadDisk::thread_disk()->cancel_callback(id); }
void            cancel_callback_and_wait(system::callback_id& id)                        { ThreadDisk::thread_disk()->cancel_callback_and_wait(id); }

} // namespace disk_thread


ThreadDisk* ThreadDisk::m_thread_disk{nullptr};

ThreadDisk::~ThreadDisk() = default;

void
ThreadDisk::create_thread() {
  assert(m_thread_disk == nullptr && "ThreadDisk already created.");

  m_thread_disk = new ThreadDisk;

  m_thread_disk->m_hash_check_queue = std::make_unique<HashCheckQueue>();
}

void
ThreadDisk::destroy_thread() {
  try {
    delete m_thread_disk;
    m_thread_disk = nullptr;
  } catch (...) {
    m_thread_disk = nullptr;
  }
}

ThreadDisk*
ThreadDisk::thread_disk() {
  return m_thread_disk;
}

void
ThreadDisk::init_thread() {
  m_resolver = std::make_unique<net::Resolver>();
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_DISK - INSTRUMENTATION_POLLING_DO_POLL;

  m_hash_check_queue->slot_chunk_done() = [](auto hc, const auto& hv) {
      ThreadMain::thread_main()->hash_queue()->chunk_done(hc, hv);
    };
}

void
ThreadDisk::cleanup_thread() {
  // Drain pending closes so we do not leak fds at shutdown.
  perform_close_fds();

  assert(m_hash_check_queue->empty() && "ThreadDisk::cleanup_thread(): m_hash_check_queue not empty.");
  assert(m_close_fds.empty() && "ThreadDisk::cleanup_thread(): m_close_fds not empty.");
}

void
ThreadDisk::queue_close_fd(int fd) {
  if (fd < 0)
    return;

  {
    auto lock = std::lock_guard(m_close_fds_lock);
    m_close_fds.push_back(fd);
  }

  interrupt();
}

void
ThreadDisk::queue_close_fds(const std::vector<int>& fds) {
  if (fds.empty())
    return;

  {
    auto lock = std::lock_guard(m_close_fds_lock);
    m_close_fds.insert(m_close_fds.end(), fds.begin(), fds.end());
  }

  interrupt();
}

void
ThreadDisk::perform_close_fds() {
  std::deque<int> local;

  {
    auto lock = std::lock_guard(m_close_fds_lock);
    local.swap(m_close_fds);
  }

  for (int fd : local)
    ::close(fd);
}

void
ThreadDisk::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got thread_disk tick.");

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  perform_close_fds();
  process_callbacks();
}

std::chrono::microseconds
ThreadDisk::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
