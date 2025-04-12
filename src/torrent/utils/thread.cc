#include "config.h"

#include "torrent/utils/thread.h"

#include <cassert>
#include <cstring>
#include <signal.h>
#include <unistd.h>

#include "globals.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/thread_interrupt.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent::utils {

thread_local Thread*     Thread::m_self{nullptr};
Thread::global_lock_type Thread::m_global;

Thread::Thread() :
  m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL)
{
  std::memset(&m_thread, 0, sizeof(pthread_t));

  thread_interrupt::pair_type interrupt_sockets = thread_interrupt::create_pair();

  m_interrupt_sender = std::move(interrupt_sockets.first);
  m_interrupt_receiver = std::move(interrupt_sockets.second);
}

Thread::~Thread() {
  // Disown m_poll instead of deleting it as we don't properly clean up all the sockets.
  m_poll.release();
}

Thread*
Thread::self() {
  return m_self;
}

void
Thread::start_thread() {
  if (m_poll == nullptr)
    throw internal_error("No poll object for thread defined.");

  if (!is_initialized())
    throw internal_error("Called Thread::start_thread on an uninitialized object.");

  if (pthread_create(&m_thread, NULL, (pthread_func)&Thread::event_loop, this))
    throw internal_error("Failed to create thread.");

  while (m_state != STATE_ACTIVE)
    usleep(100);
}

void
Thread::stop_thread() {
  m_flags |= flag_do_shutdown;
  interrupt();
}

void
Thread::stop_thread_wait() {
  stop_thread();

  release_global_lock();

  pthread_join(m_thread, NULL);
  assert(is_inactive());

  acquire_global_lock();
}

void
Thread::callback(void* target, std::function<void ()>&& fn) {
  {
    std::lock_guard<std::mutex> guard(m_callbacks_lock);

    m_callbacks.emplace(target, std::move(fn));
  }

  interrupt();
}

void
Thread::cancel_callback(void* target) {
  if (target == nullptr)
    throw internal_error("Thread::cancel_callback called with a null pointer target.");

  std::lock_guard<std::mutex> guard(m_callbacks_lock);

  m_callbacks.erase(target);
}

void
Thread::cancel_callback_and_wait(void* target) {
  cancel_callback(target);

  if (std::this_thread::get_id() != m_thread_id && m_callbacks_processing)
    std::unique_lock<std::mutex> lock(m_callbacks_processing_lock);
}

// Fix interrupting when shutting down thread.
void
Thread::interrupt() {
  // Only poke when polling, set no_timeout
  if (is_polling())
    m_interrupt_sender->poke();
}

bool
Thread::should_handle_sigusr1() {
  return false;
}

void*
Thread::event_loop(Thread* thread) {
  if (thread == nullptr)
    throw internal_error("Thread::event_loop called with a null pointer thread");

  m_self = thread;

  thread->m_thread_id = std::this_thread::get_id();
  thread->m_signal_bitfield.handover(std::this_thread::get_id());

  if (thread->m_resolver)
    thread->m_resolver->init();

  auto previous_state = STATE_INITIALIZED;

#if defined(HAS_PTHREAD_SETNAME_NP_DARWIN)
  pthread_setname_np(thread->name());
#elif defined(HAS_PTHREAD_SETNAME_NP_GENERIC)
  // Cannot use thread->m_thread here as it may not be set before pthread_create returns.
  pthread_setname_np(pthread_self(), thread->name());
#endif

  if (!thread->m_state.compare_exchange_strong(previous_state, STATE_ACTIVE))
    throw internal_error("Thread::event_loop called on an object that is not in the initialized state.");

  lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Starting thread.", thread->name());

  try {

    thread->m_poll->insert_read(thread->m_interrupt_receiver.get());

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

    thread->m_poll->remove_read(thread->m_interrupt_receiver.get());

  } catch (torrent::shutdown_exception& e) {
    lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Shutting down thread.", thread->name());
  }

  previous_state = STATE_ACTIVE;

  if (!thread->m_state.compare_exchange_strong(previous_state, STATE_INACTIVE))
    throw internal_error("Thread::event_loop called on an object that is not in the active state.");

  return NULL;
}

void
Thread::process_callbacks() {
  while (true) {
    std::function<void ()> callback;

    {
      std::lock_guard<std::mutex> guard(m_callbacks_lock);

      if (m_callbacks.empty())
        break;

      callback = m_callbacks.extract(m_callbacks.begin()).mapped();

      // The 'm_callbacks_processing_lock' is used by 'cancel_callback_and_wait' as a way to wait
      // for the processing of the callbacks to finish.
      m_callbacks_processing_lock.lock();
      m_callbacks_processing = true;
    }

    callback();

    m_callbacks_processing = false;
    m_callbacks_processing_lock.unlock();
  }
}

}
