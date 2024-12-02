#include "config.h"

#include <rak/timer.h>

#include "thread_main.h"

#include "globals.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

void
thread_main::init_thread() {
  acquire_global_lock();

  if (!Poll::slot_create_poll())
    throw internal_error("thread_main::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = Poll::slot_create_poll()();
  m_poll->set_flags(Poll::flag_waive_global_lock);

  m_state = STATE_INITIALIZED;
  m_thread = pthread_self();
  m_flags |= flag_main_thread;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;
}

void
thread_main::call_events() {
  cachedTime = rak::timer::current();

  // Ensure we don't call rak::timer::current() twice if there was no
  // scheduled tasks called.
  if (taskScheduler.empty() || taskScheduler.top()->time() > cachedTime)
    return;

  while (!taskScheduler.empty() && taskScheduler.top()->time() <= cachedTime) {
    rak::priority_item* v = taskScheduler.top();
    taskScheduler.pop();

    v->clear_time();
    v->slot()();
  }

  // Update the timer again to ensure we get accurate triggering of
  // msec timers.
  cachedTime = rak::timer::current();
}

int64_t
thread_main::next_timeout_usec() {
  cachedTime = rak::timer::current();

  if (!taskScheduler.empty())
    return std::max(taskScheduler.top()->time() - cachedTime, rak::timer()).usec();
  else
    return rak::timer::from_seconds(60).usec();
}

}
