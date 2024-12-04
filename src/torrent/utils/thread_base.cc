#include "config.h"

#include <cstring>
#include <signal.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/thread_base.h"
#include "torrent/utils/thread_interrupt.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

thread_base::global_lock_type thread_base::m_global;

thread_base::thread_base() :
  m_state(STATE_UNKNOWN),
  m_flags(0),
  m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL),

  m_poll(NULL)
{
  std::memset(&m_thread, 0, sizeof(pthread_t));

// #ifdef USE_INTERRUPT_SOCKET
  thread_interrupt::pair_type interrupt_sockets = thread_interrupt::create_pair();

  m_interrupt_sender = std::move(interrupt_sockets.first);
  m_interrupt_receiver = std::move(interrupt_sockets.second);

// #endif
}

thread_base::~thread_base() = default;

void
thread_base::start_thread() {
  if (m_poll == nullptr)
    throw internal_error("No poll object for thread defined.");

  if (!is_initialized())
    throw internal_error("Called thread_base::start_thread on an uninitialized object.");

  if (pthread_create(&m_thread, NULL, (pthread_func)&thread_base::event_loop, this))
    throw internal_error("Failed to create thread.");
}

void
thread_base::stop_thread() {
  m_flags |= flag_do_shutdown;
  interrupt();
}

void
thread_base::stop_thread_wait() {
  stop_thread();

  release_global_lock();

  while (!is_inactive()) {
    usleep(1000);
  }

  acquire_global_lock();
}

// Fix interrupting when shutting down thread.
void
thread_base::interrupt() {
  // Only poke when polling, set no_timeout
  if (is_polling())
    m_interrupt_sender->poke();
}

bool
thread_base::should_handle_sigusr1() {
  return false;
}

void*
thread_base::event_loop(thread_base* thread) {
  if (thread == nullptr)
    throw internal_error("thread_base::event_loop called with a null pointer thread");

  auto previous_state = STATE_INITIALIZED;

  if (!thread->m_state.compare_exchange_strong(previous_state, STATE_ACTIVE))
    throw internal_error("thread_base::event_loop called on an object that is not in the initialized state.");

#if defined(HAS_PTHREAD_SETNAME_NP_DARWIN)
  pthread_setname_np(thread->name());
#elif defined(HAS_PTHREAD_SETNAME_NP_GENERIC)
  pthread_setname_np(thread->m_thread, thread->name());
#endif

  lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Starting thread.", thread->name());

  thread->m_signal_bitfield.handover(std::this_thread::get_id());

  try {

// #ifdef USE_INTERRUPT_SOCKET
    thread->m_poll->insert_read(thread->m_interrupt_receiver.get());
// #endif

    while (true) {
      if (thread->m_slot_do_work)
        thread->m_slot_do_work();

      thread->call_events();
      thread->signal_bitfield()->work();

      thread->m_flags |= flag_polling;

      // Call again after setting flag_polling to ensure we process
      // any events set while it was working.
      if (thread->m_slot_do_work)
        thread->m_slot_do_work();

      thread->call_events();
      thread->signal_bitfield()->work();

      uint64_t next_timeout = 0;

      if (!thread->has_no_timeout()) {
        next_timeout = thread->next_timeout_usec();

        if (thread->m_slot_next_timeout)
          next_timeout = std::min(next_timeout, thread->m_slot_next_timeout());
      }

      // Add the sleep call when testing interrupts, etc.
      // usleep(50);

      int poll_flags = 0;

      if (!(thread->flags() & flag_main_thread))
        poll_flags = torrent::Poll::poll_worker_thread;

      instrumentation_update(INSTRUMENTATION_POLLING_DO_POLL, 1);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_DO_POLL + thread->m_instrumentation_index), 1);

      int event_count = thread->m_poll->do_poll(next_timeout, poll_flags);

      instrumentation_update(INSTRUMENTATION_POLLING_EVENTS, event_count);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_EVENTS + thread->m_instrumentation_index), event_count);

      thread->m_flags &= ~(flag_polling | flag_no_timeout);
    }

// #ifdef USE_INTERRUPT_SOCKET
    thread->m_poll->remove_write(thread->m_interrupt_receiver.get());
// #endif

  } catch (torrent::shutdown_exception& e) {
    lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Shutting down thread.", thread->name());
  }

  previous_state = STATE_ACTIVE;

  if (!thread->m_state.compare_exchange_strong(previous_state, STATE_INACTIVE))
    throw internal_error("thread_base::event_loop called on an object that is not in the active state.");

  return NULL;
}

}
