#include "config.h"

#include "torrent/utils/thread.h"

#include <cassert>
#include <cstring>
#include <mutex>
#include <signal.h>
#include <unistd.h>

#include "globals.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/thread_interrupt.h"
#include "torrent/utils/log.h"
#include "torrent/utils/scheduler.h"
#include "utils/instrumentation.h"

namespace torrent::utils {

thread_local Thread*     Thread::m_self{nullptr};
Thread::global_lock_type Thread::m_global;

Thread::Thread() :
  m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL),
  m_scheduler(std::make_unique<Scheduler>())
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

  if (pthread_create(&m_thread, NULL, reinterpret_cast<pthread_func>(&Thread::enter_event_loop), this))
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
    auto lock = std::scoped_lock(m_callbacks_lock);

    m_callbacks.emplace(target, std::move(fn));
  }

  interrupt();
}

void
Thread::cancel_callback(void* target) {
  if (target == nullptr)
    throw internal_error("Thread::cancel_callback called with a null pointer target.");

  auto lock = std::scoped_lock(m_callbacks_lock);

  m_callbacks.erase(target);
}

void
Thread::cancel_callback_and_wait(void* target) {
  cancel_callback(target);

  if (std::this_thread::get_id() != m_thread_id && m_callbacks_processing)
    auto lock = std::scoped_lock(m_callbacks_processing_lock);
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
Thread::enter_event_loop(Thread* thread) {
  if (thread == nullptr)
    throw internal_error("Thread::enter_event_loop called with a null pointer thread");

  thread->init_thread_local();
  thread->event_loop();

  return nullptr;
}

void
Thread::event_loop() {
  lt_log_print(torrent::LOG_THREAD_NOTICE, "%s : starting thread event loop", name());

  try {

    m_poll->insert_read(m_interrupt_receiver.get());

    while (true) {
      process_events();

      m_flags |= flag_polling;

      // Call again after setting flag_polling to ensure we process any events that have
      // race-conditions with flag_polling.
      process_events();

      auto timeout = next_timeout();

      instrumentation_update(INSTRUMENTATION_POLLING_DO_POLL, 1);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_DO_POLL + m_instrumentation_index), 1);

      int poll_flags = 0;

      if (!(flags() & flag_main_thread))
        poll_flags = torrent::Poll::poll_worker_thread;

      int event_count = m_poll->do_poll(timeout.count(), poll_flags);

      instrumentation_update(INSTRUMENTATION_POLLING_EVENTS, event_count);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_EVENTS + m_instrumentation_index), event_count);

      m_flags &= ~flag_polling;
    }

    m_poll->remove_read(m_interrupt_receiver.get());

  } catch (torrent::shutdown_exception& e) {
    lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Shutting down thread.", name());
  }

  auto previous_state = STATE_ACTIVE;

  if (!m_state.compare_exchange_strong(previous_state, STATE_INACTIVE))
    throw internal_error("Thread::event_loop called on an object that is not in the active state.");
}

void
Thread::init_thread_local() {
  m_self = this;
  m_thread = pthread_self();
  m_thread_id = std::this_thread::get_id();

#if defined(HAS_PTHREAD_SETNAME_NP_DARWIN)
  pthread_setname_np(name());
#elif defined(HAS_PTHREAD_SETNAME_NP_GENERIC)
  // Cannot use thread->m_thread here as it may not be set before pthread_create returns.
  pthread_setname_np(pthread_self(), name());
#endif

  m_signal_bitfield.handover(std::this_thread::get_id());

  if (m_resolver)
    m_resolver->init();

  auto previous_state = STATE_INITIALIZED;

  if (!m_state.compare_exchange_strong(previous_state, STATE_ACTIVE))
    throw internal_error("Thread::init_thread_local() : " + std::string(name()) + " : called on an object that is not in the initialized state.");
}

void
Thread::process_events() {
  m_cached_time = time_since_epoch();
  call_events();

  m_signal_bitfield.work();

  m_cached_time = time_since_epoch();
  m_scheduler->perform(m_cached_time);
}

// TODO: This should be called in event_loop or process_events.
void
Thread::process_callbacks() {
  while (true) {
    std::function<void ()> callback;

    {
      auto lock = std::scoped_lock(m_callbacks_lock);

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
