#include "config.h"

#include <rak/timer.h>

#include "thread_tracker.h"

#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/tracker/manager.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

void
ThreadTracker::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadTracker::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = Poll::slot_create_poll()();
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_TRACKER - INSTRUMENTATION_POLLING_DO_POLL;
}

void
ThreadTracker::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got ThreadTracker tick.");

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  // TODO: Add as signal.
  // m_manager->process_events();
}

int64_t
ThreadTracker::next_timeout_usec() {
  return rak::timer::from_minutes(60).round_seconds().usec();
}

}
