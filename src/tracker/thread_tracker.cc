#include "config.h"

#include "tracker/thread_tracker.h"

#include <cassert>

#include "tracker/udp_router.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "torrent/runtime/network_config.h"
#include "torrent/tracker/manager.h"
#include "utils/instrumentation.h"

namespace torrent {

namespace tracker_thread {

system::Thread* thread()                                                       { return ThreadTracker::thread_base(); }
std::thread::id thread_id()                                                    { return ThreadTracker::thread_base()->thread_id(); }

void            callback(void* target, std::function<void ()>&& fn)            { ThreadTracker::thread_base()->callback(target, std::move(fn)); }
void            cancel_callback(void* target)                                  { ThreadTracker::thread_base()->cancel_callback(target); }

void            callback(std::function<void ()>&& fn)                          { ThreadTracker::thread_base()->callback(std::move(fn)); }
void            callback(system::callback_id& id, std::function<void ()>&& fn) { ThreadTracker::thread_base()->callback(id, std::move(fn)); }
void            cancel_callback(system::callback_id& id)                       { ThreadTracker::thread_base()->cancel_callback(id); }
void            cancel_callback_and_wait(system::callback_id& id)              { ThreadTracker::thread_base()->cancel_callback_and_wait(id); }

tracker::Manager* manager()                                                    { return ThreadTracker::thread_tracker()->tracker_manager(); }

} // namespace tracker


ThreadTracker* ThreadTracker::m_thread_tracker{};

ThreadTracker::~ThreadTracker() = default;

void
ThreadTracker::create_thread() {
  assert(m_thread_tracker == nullptr);

  m_thread_tracker = new ThreadTracker();

  m_thread_tracker->m_resolver         = std::make_unique<net::Resolver>();
  m_thread_tracker->m_tracker_manager  = std::make_unique<tracker::Manager>();
  m_thread_tracker->m_udp_inet_router  = std::make_unique<tracker::UdpRouter>();
  m_thread_tracker->m_udp_inet6_router = std::make_unique<tracker::UdpRouter>();
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
  m_thread_tracker->m_udp_inet_router->open(AF_INET);
  m_thread_tracker->m_udp_inet6_router->open(AF_INET6);

  runtime::network_config()->subscribe_to_changes(this, [this]() {
      cancel_callback(this);

      callback(this, [this]() {
          m_udp_inet_router->updated_network_config(AF_INET);
          m_udp_inet6_router->updated_network_config(AF_INET6);
        });
    });
}

void
ThreadTracker::cleanup_thread() {
  m_tracker_manager.reset();

  runtime::network_config()->unsubscribe_from_changes(this);

  m_udp_inet_router->close();
  m_udp_inet6_router->close();

  m_udp_inet_router.reset();
  m_udp_inet6_router.reset();
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
