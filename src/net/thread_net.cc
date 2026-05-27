#include "config.h"

#include "net/thread_net.h"

#include "net/curl_stack.h"
#include "net/dns_buffer.h"
#include "net/dns_cache.h"
#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/callbacks.h"
#include "utils/instrumentation.h"

namespace torrent {

class ThreadNetInternal {
public:
  static net::HttpStack* http_stack() { return ThreadNet::thread_net()->http_stack(); }
};

namespace net_thread {

torrent::system::Thread* thread()                                                                 { return ThreadNet::thread_net(); }
std::thread::id          thread_id()                                                              { return ThreadNet::thread_net()->thread_id(); }

void                     callback(std::function<void ()>&& fn)                                    { ThreadNet::thread_net()->callback(std::move(fn)); }
void                     callback(system::callback_id& id, std::function<void ()>&& fn)           { ThreadNet::thread_net()->callback(id, std::move(fn)); }
void                     callback_interrupt(std::function<void ()>&& fn)                          { ThreadNet::thread_net()->callback_interrupt(std::move(fn)); }
void                     callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) { ThreadNet::thread_net()->callback_interrupt(id, std::move(fn)); }
void                     cancel_callback(system::callback_id& id)                                 { ThreadNet::thread_net()->cancel_callback(id); }

torrent::net::HttpStack* http_stack()                                                             { return ThreadNetInternal::http_stack(); }

} // namespace net_thread

namespace {

uint32_t
calculate_http_host_connections(uint32_t max_size) {
  if (max_size >= 16384)
    return 3;
  else if (max_size >= 8096)
    return 2;
  else
    return 1;
}

} // namespace


ThreadNet* ThreadNet::m_thread_net{};

ThreadNet::~ThreadNet() = default;

void
ThreadNet::create_thread() {
  auto thread = new ThreadNet;

  thread->m_events_callback_id = system::make_callback_id();
  thread->m_http_stack         = std::make_unique<net::HttpStack>(thread);
  thread->m_dns_buffer         = std::make_unique<net::DnsBuffer>();
  thread->m_dns_cache          = std::make_unique<net::DnsCache>();
  thread->m_dns_resolver       = std::make_unique<net::UdnsResolver>();

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

  set_max_connections();
}

void
ThreadNet::init_thread_post_local() {
  m_dns_resolver->initialize(this);

  runtime::socket_manager()->subscribe_to_changes(this, [this]() {
      callback(m_events_callback_id, ThreadNet::set_max_connections);
    });
}

void
ThreadNet::cleanup_thread() {
  runtime::socket_manager()->unsubscribe_from_changes(this);

  m_http_stack->shutdown();
  m_dns_resolver->cleanup();
}

void
ThreadNet::set_max_connections() {
  // TODO: Also set max cache connections?

  auto total_size = runtime::socket_manager()->category_max_size(runtime::category_http);
  auto host_size  = calculate_http_host_connections(runtime::socket_manager()->max_size());

  ThreadNet::thread_net()->m_http_stack->set_max_total_connections(total_size);
  ThreadNet::thread_net()->m_http_stack->set_max_host_connections(host_size);
}

void
ThreadNet::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got thread_disk tick.");

  while (m_http_stack->curl_stack()->process_done_handle())
    ; // Do nothing.

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  process_callbacks();
  m_dns_resolver->flush();
  process_callbacks();
}

std::chrono::microseconds
ThreadNet::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
