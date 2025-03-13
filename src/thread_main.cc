#include "config.h"

#include <rak/timer.h>

#include "thread_main.h"

#include "globals.h"
#include "data/hash_queue.h"
#include "data/thread_disk.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadMain* thread_main = nullptr;

ThreadMain::ThreadMain() {
  m_hash_queue = std::make_unique<HashQueue>();
}

void
ThreadMain::init_thread() {
  acquire_global_lock();

  if (!Poll::slot_create_poll())
    throw internal_error("ThreadMain::init_thread(): Poll::slot_create_poll() not valid.");

  thread_self = this;

  m_poll = Poll::slot_create_poll()();
  m_poll->set_flags(Poll::flag_waive_global_lock);

  m_state = STATE_INITIALIZED;
  m_thread = pthread_self();
  m_thread_id = std::this_thread::get_id();
  m_flags |= flag_main_thread;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;

  auto hash_work_signal = m_signal_bitfield.add_signal([this]() {
      return m_hash_queue->work();
    });
  m_hash_queue->slot_has_work() = [this, hash_work_signal](bool is_done) {
      send_event_signal(hash_work_signal, is_done);
    };

  thread_disk->hash_check_queue()->slot_chunk_done() = [this](auto hc, const auto& hv) {
      m_hash_queue->chunk_done(hc, hv);
    };
}

void
ThreadMain::call_events() {
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
ThreadMain::next_timeout_usec() {
  cachedTime = rak::timer::current();

  if (!taskScheduler.empty())
    return std::max(taskScheduler.top()->time() - cachedTime, rak::timer()).usec();
  else
    return rak::timer::from_seconds(60).usec();
}

}
