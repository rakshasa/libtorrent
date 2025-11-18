#include "config.h"

#include "data/thread_disk.h"

#include <cassert>

#include "thread_main.h"
#include "data/hash_queue.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadDisk* ThreadDisk::m_thread_disk{nullptr};

ThreadDisk::~ThreadDisk() = default;

void
ThreadDisk::create_thread() {
  assert(m_thread_disk == nullptr && "ThreadDisk already created.");

  m_thread_disk = new ThreadDisk;
}

void
ThreadDisk::destroy_thread() {
  delete m_thread_disk;
  m_thread_disk = nullptr;
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

  m_hash_check_queue.slot_chunk_done() = [](auto hc, const auto& hv) {
      ThreadMain::thread_main()->hash_queue()->chunk_done(hc, hv);
    };
}

void
ThreadDisk::cleanup_thread() {
  assert(m_hash_check_queue.empty() && "ThreadDisk::cleanup_thread(): m_hash_check_queue not empty.");
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

  m_hash_check_queue.perform();
  process_callbacks();
}

std::chrono::microseconds
ThreadDisk::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
