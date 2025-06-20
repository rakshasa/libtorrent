#include "config.h"

#include "torrent/utils/thread.h"

#include <cassert>
#include <cstring>
#include <mutex>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "torrent/poll.h"
#include "torrent/utils/chrono.h"
#include "torrent/utils/log.h"
#include "torrent/utils/scheduler.h"
#include "utils/instrumentation.h"
#include "utils/signal_interrupt.h"
#include "utils/thread_internal.h"

namespace torrent::utils {

thread_local Thread* Thread::m_self{nullptr};

Thread::Thread() :
    m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL),
    m_poll(Poll::create()),
    m_scheduler(new Scheduler) {

  std::tie(m_interrupt_sender, m_interrupt_receiver) = SignalInterrupt::create_pair();

  m_cached_time = time_since_epoch();
  m_scheduler->set_cached_time(m_cached_time);
}

Thread::~Thread() = default;

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

  if (pthread_create(&m_thread, NULL, &Thread::enter_event_loop, this))
    throw internal_error("Failed to create thread.");

  while (m_state != STATE_ACTIVE)
    usleep(100);
}

// Each thread needs to check flag_do_shutdown in call_events() and decide how to cleanly shut down.
void
Thread::stop_thread_wait() {
  m_flags |= flag_do_shutdown;
  interrupt();

  pthread_join(m_thread, NULL);
  assert(is_inactive());
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
Thread::callback_interrupt_pollling(void* target, std::function<void ()>&& fn) {
  {
    auto lock = std::scoped_lock(m_callbacks_lock);

    m_interrupt_callbacks.emplace(target, std::move(fn));
    m_callbacks_should_interrupt_polling = true;
  }

  interrupt();
}

void
Thread::cancel_callback(void* target) {
  if (target == nullptr)
    throw internal_error("Thread::cancel_callback called with a null pointer target.");

  auto lock = std::scoped_lock(m_callbacks_lock);

  m_callbacks.erase(target);
  m_interrupt_callbacks.erase(target);
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
Thread::enter_event_loop(void* thread) {
  auto t = static_cast<Thread*>(thread);
  if (t == nullptr)
    throw internal_error("Thread::enter_event_loop called with a null pointer thread");

  t->init_thread_local();
  t->event_loop();
  t->cleanup_thread_local();

  return nullptr;
}

void
Thread::event_loop() {
  lt_log_print(LOG_THREAD_NOTICE, "%s : starting thread event loop", name());

  try {

    m_poll->insert_read(m_interrupt_receiver.get());

    while (true) {
      process_events();

      m_flags |= flag_polling;

      // Call again after setting flag_polling to ensure we process any events that have
      // race-conditions with flag_polling.
      process_events();

      instrumentation_update(INSTRUMENTATION_POLLING_DO_POLL, 1);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_DO_POLL + m_instrumentation_index), 1);

      auto timeout = std::max(next_timeout(), std::chrono::microseconds(0));

      if (!m_scheduler->empty())
        timeout = std::min(timeout, m_scheduler->next_timeout());

      int event_count = m_poll->do_poll(timeout.count());

      instrumentation_update(INSTRUMENTATION_POLLING_EVENTS, event_count);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_EVENTS + m_instrumentation_index), event_count);

      m_flags &= ~flag_polling;
    }

  } catch (const shutdown_exception&) {
    lt_log_print(LOG_THREAD_NOTICE, "%s: Shutting down thread.", name());
  }

  // Some test, and perhaps other code, segfaults on this.
  // TODO: Test
  //m_poll->remove_read(m_interrupt_receiver.get());

  auto previous_state = STATE_ACTIVE;

  if (!m_state.compare_exchange_strong(previous_state, STATE_INACTIVE))
    throw internal_error("Thread::event_loop called on an object that is not in the active state.");
}

void
Thread::init_thread_local() {
#if defined(HAS_PTHREAD_SETNAME_NP_DARWIN)
  pthread_setname_np(name());
#elif defined(HAS_PTHREAD_SETNAME_NP_GENERIC)
  // Cannot use thread->m_thread here as it may not be set before pthread_create returns.
  pthread_setname_np(pthread_self(), name());
#endif

  m_self = this;
  m_thread = pthread_self();
  m_thread_id = std::this_thread::get_id();

  m_scheduler->set_thread_id(m_thread_id);
  m_signal_bitfield.handover(m_thread_id);

  set_cached_time(time_since_epoch());

  if (m_resolver)
    m_resolver->init();

  auto previous_state = STATE_INITIALIZED;

  if (!m_state.compare_exchange_strong(previous_state, STATE_ACTIVE))
    throw internal_error("Thread::init_thread_local() : " + std::string(name()) + " : called on an object that is not in the initialized state.");
}

void
Thread::cleanup_thread_local() {
  lt_log_print(LOG_THREAD_NOTICE, "%s : cleaning up thread local data", name());

  cleanup_thread();

  // TODO: Cleanup the resolver, scheduler, and poll objects.
  m_self = nullptr;
}

void
Thread::set_cached_time(std::chrono::microseconds t) {
  m_cached_time = t;
  m_scheduler->set_cached_time(t);
}

void
Thread::process_events() {
  // TODO: We should call process_callbacks() here before and after call_events, however due to the
  // many different cached times in the code, we need to let each thread manage this themselves.

  set_cached_time(time_since_epoch());

  call_events();
  m_signal_bitfield.work();

  set_cached_time(time_since_epoch());

  m_scheduler->perform(m_cached_time);
}

// Used for testing.
void
Thread::process_events_without_cached_time() {
  call_events();
  m_signal_bitfield.work();
  m_scheduler->perform(m_cached_time);
}

// TODO: This should be called in process_events.
void
Thread::process_callbacks(bool only_interrupt) {
  m_callbacks_should_interrupt_polling = false;

  while (true) {
    std::function<void ()> callback;

    {
      auto lock = std::scoped_lock(m_callbacks_lock);

      if (!m_interrupt_callbacks.empty())
        callback = m_interrupt_callbacks.extract(m_interrupt_callbacks.begin()).mapped();
      else if (!only_interrupt && !m_callbacks.empty())
        callback = m_callbacks.extract(m_callbacks.begin()).mapped();
      else
        break;

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

} // namespace torrent::utils

namespace torrent::this_thread {

torrent::utils::Thread*   thread()                                            { return utils::ThreadInternal::thread(); }
std::thread::id           thread_id()                                         { return utils::ThreadInternal::thread_id(); }

std::chrono::microseconds cached_time()                                       { return utils::ThreadInternal::cached_time(); }
std::chrono::seconds      cached_seconds()                                    { return utils::ThreadInternal::cached_seconds(); }

void                      callback(void* target, std::function<void ()>&& fn) { utils::ThreadInternal::callback(target, std::move(fn)); }
void                      cancel_callback(void* target)                       { utils::ThreadInternal::cancel_callback(target); }
void                      cancel_callback_and_wait(void* target)              { utils::ThreadInternal::cancel_callback_and_wait(target); }

Poll*                     poll()                                              { return utils::ThreadInternal::poll(); }
net::Resolver*            resolver()                                          { return utils::ThreadInternal::resolver(); }
utils::Scheduler*         scheduler()                                         { return utils::ThreadInternal::scheduler(); }

} // namespace torrent::this_thread
