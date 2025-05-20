#include "config.h"

#include "thread_main.h"

#include "globals.h"
#include "data/hash_queue.h"
#include "data/thread_disk.h"
#include "rak/timer.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/chrono.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadMain* ThreadMain::m_thread_main{nullptr};

ThreadMain::~ThreadMain() {
  m_thread_main = nullptr;

  m_hash_queue.reset();
}

void
ThreadMain::create_thread() {
  m_thread_main = new ThreadMain;

  m_thread_main->m_hash_queue = std::make_unique<HashQueue>();
}

ThreadMain*
ThreadMain::thread_main() {
  return m_thread_main;
}

void
ThreadMain::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadMain::init_thread(): Poll::slot_create_poll() not valid.");

  m_resolver = std::make_unique<net::Resolver>();
  m_poll = std::unique_ptr<Poll>(Poll::slot_create_poll()());
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;

  init_thread_local();

  // We should only initialize things here that depend on main thread, as we want to call
  // 'init_thread()' before 'torrent::initalize()'.

  auto hash_work_signal = m_signal_bitfield.add_signal([this]() {
      return m_hash_queue->work();
    });

  m_hash_queue->slot_has_work() = [this, hash_work_signal](bool is_done) {
      send_event_signal(hash_work_signal, is_done);
    };
}

void
ThreadMain::call_events() {
  cachedTime = rak::timer::current();

  if (m_slot_do_work)
    m_slot_do_work();

  while (!taskScheduler.empty() && taskScheduler.top()->time() <= cachedTime) {
    rak::priority_item* v = taskScheduler.top();
    taskScheduler.pop();

    v->clear_time();
    v->slot()();
  }

  // Update the timer again to ensure we get accurate triggering of
  // msec timers.
  cachedTime = rak::timer::current();

  process_callbacks();
}

std::chrono::microseconds
ThreadMain::next_timeout() {
  cachedTime = rak::timer::current();

  if (taskScheduler.empty())
    return 10s;

  auto task_timeout = std::max(taskScheduler.top()->time() - cachedTime, rak::timer()).usec();

  return std::min(std::chrono::microseconds(task_timeout), std::chrono::microseconds(10s));
}

}
