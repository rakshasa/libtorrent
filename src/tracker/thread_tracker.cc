#include "config.h"

#include "tracker/thread_tracker.h"

#include <cassert>

#include "tracker/udp_router.h"
#include "torrent/exceptions.h"
#include "torrent/tracker/manager.h"
#include "utils/instrumentation.h"

#include "thread_main.h"

namespace torrent {

namespace tracker_thread {

torrent::system::Thread* thread()    { return ThreadTracker::thread_tracker(); }
std::thread::id          thread_id() { return ThreadTracker::thread_tracker()->thread_id(); }

} // namespace tracker


ThreadTracker* ThreadTracker::m_thread_tracker{};

ThreadTracker::~ThreadTracker() = default;

void
ThreadTracker::create_thread(system::Thread* main_thread) {
  assert(m_thread_tracker == nullptr);

  m_thread_tracker = new ThreadTracker();

  m_thread_tracker->m_tracker_manager  = std::make_unique<tracker::Manager>(main_thread, m_thread_tracker);
  // m_thread_tracker->m_udp_inet_router  = std::make_unique<tracker::UdpRouter>();
  // m_thread_tracker->m_udp_inet6_router = std::make_unique<tracker::UdpRouter>();
}

void
ThreadTracker::destroy_thread() {
  delete m_thread_tracker;
  m_thread_tracker = nullptr;
}

ThreadTracker*
ThreadTracker::thread_tracker() {
  return m_thread_tracker;
}

void
ThreadTracker::init_thread() {
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_TRACKER - INSTRUMENTATION_POLLING_DO_POLL;
}

void
ThreadTracker::init_thread_post_local() {
  // m_thread_tracker->m_udp_inet_router->open(AF_INET);
  // m_thread_tracker->m_udp_inet6_router->open(AF_INET6);
}

void
ThreadTracker::cleanup_thread() {
  m_tracker_manager.reset();

  // m_udp_inet_router->close();
  // m_udp_inet_router.reset();

  // m_udp_inet6_router->close();
  // m_udp_inet6_router.reset();
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

  // TODO: Do we need to process scheduled events here?

  process_callbacks();
}

std::chrono::microseconds
ThreadTracker::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
