#include "config.h"

#include "net/thread_net.h"

#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

using namespace std::chrono_literals;

namespace torrent {

ThreadNet* ThreadNet::m_thread_net{nullptr};

ThreadNet::~ThreadNet() = default;

void
ThreadNet::create_thread() {
  auto thread = new ThreadNet;

  thread->m_udns = std::make_unique<UdnsResolver>();

  m_thread_net = thread;
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
ThreadNet::cleanup_thread() {
  m_thread_net = nullptr;

  m_udns.reset();
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

}
