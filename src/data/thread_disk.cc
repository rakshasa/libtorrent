#include "config.h"

#include <rak/timer.h>

#include "data/thread_disk.h"

#include "data/hash_queue.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

// TODO: Move HashQueue to thread_main.
#include "manager.h"

namespace torrent {

ThreadDisk* thread_disk = nullptr;

void
ThreadDisk::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadDisk::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = Poll::slot_create_poll()();
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_DISK - INSTRUMENTATION_POLLING_DO_POLL;

  m_hash_check_queue.slot_chunk_done() = [](auto hc, const auto& hv) {
      manager->hash_queue()->chunk_done(hc, hv);
    };
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
}

int64_t
ThreadDisk::next_timeout_usec() {
  return rak::timer::from_seconds(10).round_seconds().usec();
}

}
