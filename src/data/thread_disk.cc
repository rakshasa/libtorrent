#include "config.h"

#include "data/thread_disk.h"

#include "rak/timer.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadDisk* ThreadDisk::m_thread_disk{nullptr};

ThreadDisk::~ThreadDisk() {
  m_thread_disk = nullptr;
}

void
ThreadDisk::create_thread() {
  m_thread_disk = new ThreadDisk;
}

void
ThreadDisk::destroy_thread() {
  if (m_thread_disk == nullptr)
    return;

  m_thread_disk->stop_thread_wait();

  delete m_thread_disk;
  m_thread_disk = nullptr;
}

ThreadDisk*
ThreadDisk::thread_disk() {
  return m_thread_disk;
}

void
ThreadDisk::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadDisk::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = std::unique_ptr<Poll>(Poll::slot_create_poll()());
  m_resolver = std::make_unique<net::Resolver>();

  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_DISK - INSTRUMENTATION_POLLING_DO_POLL;
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

int64_t
ThreadDisk::next_timeout_usec() {
  return rak::timer::from_seconds(10).round_seconds().usec();
}

}
