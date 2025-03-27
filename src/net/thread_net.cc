#include "config.h"

#include "net/thread_net.h"

#include "rak/timer.h"
#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadNet* thread_net = nullptr;

ThreadNet::ThreadNet() {
  m_udns = std::make_unique<UdnsResolver>();
}

void
ThreadNet::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadNet::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = std::unique_ptr<Poll>(Poll::slot_create_poll()());

  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_NET - INSTRUMENTATION_POLLING_DO_POLL;
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

int64_t
ThreadNet::next_timeout_usec() {
  return rak::timer::from_seconds(10).round_seconds().usec();
}

}
