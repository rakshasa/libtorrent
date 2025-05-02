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
#include "torrent/utils/log.h"
#include "torrent/utils/scheduler.h"
#include "utils/instrumentation.h"
#include "utils/signal_interrupt.h"

namespace torrent::utils {

thread_local Thread*     Thread::m_self{nullptr};
Thread::global_lock_type Thread::m_global;

class ThreadInternal {
public:
  static std::chrono::microseconds cached_time()    { return Thread::m_self->m_cached_time; }
  static std::chrono::seconds      cached_seconds() { return cast_seconds(Thread::m_self->m_cached_time); }
  static Poll*                     poll()           { return Thread::m_self->m_poll.get(); }
  static Scheduler*                scheduler()      { return Thread::m_self->m_scheduler.get(); }
  static net::Resolver*            resolver()       { return Thread::m_self->m_resolver.get(); }
};

Thread::Thread() :
  m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL),
  m_scheduler(std::make_unique<Scheduler>()) {

  std::tie(m_interrupt_sender, m_interrupt_receiver) = SignalInterrupt::create_pair();

  m_cached_time = time_since_epoch();
  m_scheduler->set_cached_time(m_cached_time);
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

// Each thread needs to check flag_do_shutdown in call_events() and decide how to cleanly shut down.
void
Thread::stop_thread() {
  m_flags |= flag_do_shutdown;
  interrupt();
}

void
Thread::stop_thread_wait() {
  stop_thread();

  if ((m_flags & flag_main_thread))
    release_global_lock();

  pthread_join(m_thread, NULL);
  assert(is_inactive());

  if ((m_flags & flag_main_thread))
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

      int poll_flags = 0;

      if (!(flags() & flag_main_thread))
        poll_flags = Poll::poll_worker_thread;

      auto timeout = std::max(next_timeout(), std::chrono::microseconds(0));

      if (!m_scheduler->empty())
        timeout = std::min(timeout, m_scheduler->next_timeout());

      int event_count = m_poll->do_poll(timeout.count(), poll_flags);

      instrumentation_update(INSTRUMENTATION_POLLING_EVENTS, event_count);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_EVENTS + m_instrumentation_index), event_count);

      m_flags &= ~flag_polling;
    }

  } catch (shutdown_exception& e) {
    lt_log_print(LOG_THREAD_NOTICE, "%s: Shutting down thread.", name());
  }

  // Some test, and perhaps other code, segfaults on this.
  // m_poll->remove_read(m_interrupt_receiver.get());

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

  m_cached_time = time_since_epoch();
  m_scheduler->set_cached_time(m_cached_time);
  m_scheduler->set_thread_id(m_thread_id);

  m_signal_bitfield.handover(m_thread_id);

  if (m_resolver)
    m_resolver->init();

  auto previous_state = STATE_INITIALIZED;

  if (!m_state.compare_exchange_strong(previous_state, STATE_ACTIVE))
    throw internal_error("Thread::init_thread_local() : " + std::string(name()) + " : called on an object that is not in the initialized state.");
}

void
Thread::process_events() {
  m_cached_time = time_since_epoch();
  m_scheduler->set_cached_time(m_cached_time);

  // TODO: We should call process_callbacks() here before and after call_events, however due to the
  // many different cached times in the code, we need to let each thread manage this themselves.

  call_events();

  m_signal_bitfield.work();

  m_cached_time = time_since_epoch();
  m_scheduler->set_cached_time(m_cached_time);

  m_scheduler->perform(m_cached_time);
}

// TODO: This should be called in process_events.
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

namespace torrent::this_thread {

LIBTORRENT_EXPORT std::chrono::microseconds cached_time()    { return utils::ThreadInternal::cached_time(); }
LIBTORRENT_EXPORT std::chrono::seconds      cached_seconds() { return utils::ThreadInternal::cached_seconds(); }
LIBTORRENT_EXPORT Poll*                     poll()           { return utils::ThreadInternal::poll(); }
LIBTORRENT_EXPORT net::Resolver*            resolver()       { return utils::ThreadInternal::resolver(); }
LIBTORRENT_EXPORT utils::Scheduler*         scheduler()      { return utils::ThreadInternal::scheduler(); }

}
