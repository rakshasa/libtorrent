#include "config.h"

#include "net/thread_net.h"

#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "utils/instrumentation.h"

namespace torrent {

class ThreadNetInternal {
public:
  static ThreadNet*      thread_net() { return ThreadNet::internal_thread_net(); }
  static net::HttpStack* http_stack() { return ThreadNet::internal_thread_net()->http_stack(); }
};

namespace net_thread {

torrent::utils::Thread*  thread()                                            { return ThreadNetInternal::thread_net(); }
std::thread::id          thread_id()                                         { return ThreadNetInternal::thread_net()->thread_id(); }

void                     callback(void* target, std::function<void ()>&& fn) { ThreadNetInternal::thread_net()->callback(target, std::move(fn)); }
void                     cancel_callback(void* target)                       { ThreadNetInternal::thread_net()->cancel_callback(target); }
void                     cancel_callback_and_wait(void* target)              { ThreadNetInternal::thread_net()->cancel_callback_and_wait(target); }

torrent::net::HttpStack* http_stack()                                        { return ThreadNetInternal::http_stack(); }

} // namespace net_thread

ThreadNet* ThreadNet::m_thread_net{nullptr};

ThreadNet::~ThreadNet() = default;

void
ThreadNet::create_thread() {
  auto thread = new ThreadNet;

  thread->m_http_stack = std::make_unique<net::HttpStack>(thread);
  thread->m_udns       = std::make_unique<UdnsResolver>();

  m_thread_net = thread;
}

void
ThreadNet::destroy_thread() {
  delete m_thread_net;
  m_thread_net = nullptr;
}

ThreadNet*
ThreadNet::thread_net() {
  return m_thread_net;
}

void
ThreadNet::init_thread() {
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_NET - INSTRUMENTATION_POLLING_DO_POLL;
}

void
ThreadNet::init_thread_post_local() {
  m_udns->initialize(this);
}

void
ThreadNet::cleanup_thread() {
  m_http_stack->shutdown();
  m_udns->cleanup();
}

void
ThreadNet::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got thread_disk tick.");

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  process_callbacks();
  m_udns->flush();
  process_callbacks();
}

std::chrono::microseconds
ThreadNet::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
