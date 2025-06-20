#ifndef LIBTORRENT_TORRENT_UTILS_THREAD_H
#define LIBTORRENT_TORRENT_UTILS_THREAD_H

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <pthread.h>
#include <sys/types.h>
#include <torrent/common.h>
#include <torrent/utils/signal_bitfield.h>

namespace torrent {
class SignalInterrupt;
} // namespace torrent

namespace torrent::utils {

class ThreadInternal;

class LIBTORRENT_EXPORT Thread {
public:
  using pthread_func = void* (*)(void*);

  enum state_type {
    STATE_UNKNOWN,
    STATE_INITIALIZED,
    STATE_ACTIVE,
    STATE_INACTIVE
  };

  static constexpr int flag_do_shutdown  = 0x1;
  static constexpr int flag_did_shutdown = 0x2;
  static constexpr int flag_polling      = 0x4;

  // The ctor and dtor are called outside of the thread, so thread-specific initialization and
  // destruction should be done in init_thread() and cleanup_thread() respectively.
  Thread();
  virtual ~Thread();

  // TODO: Should we clear m_self after event_loop ends?
  static Thread*      self();
  virtual const char* name() const = 0;

  bool                is_initialized() const { return state() == STATE_INITIALIZED; }
  bool                is_active()      const { return state() == STATE_ACTIVE; }
  bool                is_inactive()    const { return state() == STATE_INACTIVE; }

  bool                is_polling() const;
  bool                is_current() const;

  bool                has_do_shutdown()  const { return (flags() & flag_do_shutdown); }
  bool                has_did_shutdown() const { return (flags() & flag_did_shutdown); }

  pthread_t           pthread()            { return m_thread; }
  std::thread::id     thread_id()          { return m_thread_id; }

  state_type          state() const        { return m_state; }
  int                 flags() const        { return m_flags; }

  auto                cached_time() const  { return m_cached_time.load(); }

  // Only call these from the same thread, or before start_thread.
  //
  auto                signal_bitfield()    { return &m_signal_bitfield; }

  virtual void        init_thread() = 0;
  void                init_thread_local();

  // It is assumed that any thread-specific resources no longer are accessed at the time
  // cleanup_thread is called, or that those resources remain safe to call.
  //
  // This mean that e.g. tracker::Manager never gets called once thread_tracker is stopped.
  virtual void        cleanup_thread() = 0;
  void                cleanup_thread_local();

  void                start_thread();
  void                stop_thread_wait();

  void                callback(void* target, std::function<void ()>&& fn);
  void                callback_interrupt_pollling(void* target, std::function<void ()>&& fn);
  void                cancel_callback(void* target);
  void                cancel_callback_and_wait(void* target);

  void                interrupt();
  void                send_event_signal(unsigned int index, bool interrupt = true);

  static bool         should_handle_sigusr1();

  void                event_loop();

protected:
  friend class torrent::Poll;
  friend class ThreadInternal;

  net::Resolver*      resolver()  { return m_resolver.get(); }
  Scheduler*          scheduler() { return m_scheduler.get(); }

  void                set_cached_time(std::chrono::microseconds t);

  bool                callbacks_should_interrupt_polling() const { return m_callbacks_should_interrupt_polling.load(); }

  static void*        enter_event_loop(void* thread);

  virtual void                      call_events() = 0;
  virtual std::chrono::microseconds next_timeout() = 0;

  void                process_events();
  void                process_events_without_cached_time();
  void                process_callbacks(bool only_interrupt = false);

  static thread_local Thread*  m_self;

  // TODO: Remove m_thread.
  pthread_t                    m_thread{};
  std::atomic<std::thread::id> m_thread_id;
  std::atomic<state_type>      m_state{STATE_UNKNOWN};
  std::atomic_int              m_flags{0};

  // TODO: Make it so only thread_this can access m_cached_time.
  std::atomic<std::chrono::microseconds> m_cached_time;

  int                          m_instrumentation_index;

  std::unique_ptr<Poll>            m_poll;
  std::unique_ptr<net::Resolver>   m_resolver;
  std::unique_ptr<Scheduler>       m_scheduler;
  class signal_bitfield            m_signal_bitfield;

  std::unique_ptr<SignalInterrupt> m_interrupt_sender;
  std::unique_ptr<SignalInterrupt> m_interrupt_receiver;

  std::mutex                                         m_callbacks_lock;
  std::multimap<const void*, std::function<void ()>> m_callbacks;
  std::multimap<const void*, std::function<void ()>> m_interrupt_callbacks;
  std::atomic<bool>                                  m_callbacks_should_interrupt_polling{false};
  std::mutex                                         m_callbacks_processing_lock;
  std::atomic<bool>                                  m_callbacks_processing{false};
};

inline bool
Thread::is_polling() const {
  return (flags() & flag_polling);
}

inline bool
Thread::is_current() const {
  return m_thread == pthread_self();
}

inline void
Thread::send_event_signal(unsigned int index, bool do_interrupt) {
  m_signal_bitfield.signal(index);

  if (do_interrupt)
    interrupt();
}

} // namespace torrent::utils

#endif
