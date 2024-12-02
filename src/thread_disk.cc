#include "config.h"

#include <rak/timer.h>

#include "thread_disk.h"

#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

void
thread_disk::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("thread_disk::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = Poll::slot_create_poll()();
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_DISK - INSTRUMENTATION_POLLING_DO_POLL;
}

void
thread_disk::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got thread_disk tick.");

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  m_hash_queue.perform();
}

int64_t
thread_disk::next_timeout_usec() {
  return rak::timer::from_seconds(10).round_seconds().usec();
}

}
