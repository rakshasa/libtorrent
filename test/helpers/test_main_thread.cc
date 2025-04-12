#include "config.h"

#include "test_main_thread.h"

#include "globals.h"
#include "rak/timer.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"

TestMainThread::TestMainThread() {}

TestMainThread::~TestMainThread() {
  m_self = nullptr;
}

void
TestMainThread::init_thread() {
  // acquire_global_lock();

  if (!torrent::Poll::slot_create_poll())
    throw torrent::internal_error("ThreadMain::init_thread(): Poll::slot_create_poll() not valid.");

  m_self = this;

  m_poll = std::unique_ptr<torrent::Poll>(torrent::Poll::slot_create_poll()());
  m_poll->set_flags(torrent::Poll::flag_waive_global_lock);
  m_resolver = std::make_unique<torrent::net::Resolver>();

  m_state = STATE_INITIALIZED;
  m_thread = pthread_self();
  m_thread_id = std::this_thread::get_id();
  m_flags |= flag_main_thread;

  //m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;
}

void
TestMainThread::call_events() {
  torrent::cachedTime = rak::timer::current();

  // Ensure we don't call rak::timer::current() twice if there was no
  // scheduled tasks called.
  if (torrent::taskScheduler.empty() || torrent::taskScheduler.top()->time() > torrent::cachedTime)
    return;

  while (!torrent::taskScheduler.empty() && torrent::taskScheduler.top()->time() <= torrent::cachedTime) {
    rak::priority_item* v = torrent::taskScheduler.top();
    torrent::taskScheduler.pop();

    v->clear_time();
    v->slot()();
  }

  // Update the timer again to ensure we get accurate triggering of
  // msec timers.
  torrent::cachedTime = rak::timer::current();

  process_callbacks();
}

int64_t
TestMainThread::next_timeout_usec() {
  torrent::cachedTime = rak::timer::current();

  if (!torrent::taskScheduler.empty())
    return std::max(torrent::taskScheduler.top()->time() - torrent::cachedTime, rak::timer()).usec();
  else
    return rak::timer::from_seconds(60).usec();
}
