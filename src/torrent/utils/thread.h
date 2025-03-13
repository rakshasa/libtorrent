#ifndef LIBTORRENT_UTILS_THREADBASE_H
#define LIBTORRENT_UTILS_THREADBASE_H

#include <atomic>
#include <functional>
#include <mutex>
#include <pthread.h>
#include <sys/types.h>

#include <torrent/common.h>
#include <torrent/utils/signal_bitfield.h>

namespace torrent {
thread_local extern utils::Thread* thread_self;
}

namespace torrent::utils {

class LIBTORRENT_EXPORT Thread {
public:
  typedef void* (*pthread_func)(void*);
  typedef std::function<void ()>     slot_void;
  typedef std::function<uint64_t ()> slot_timer;
  typedef class signal_bitfield      signal_bitfield_t;

  enum state_type {
    STATE_UNKNOWN,
    STATE_INITIALIZED,
    STATE_ACTIVE,
    STATE_INACTIVE
  };

  static const int flag_do_shutdown  = 0x1;
  static const int flag_did_shutdown = 0x2;
  static const int flag_no_timeout   = 0x4;
  static const int flag_polling      = 0x8;

  static const int flag_main_thread  = 0x10;

  Thread();
  virtual ~Thread();

  bool                is_initialized() const { return state() == STATE_INITIALIZED; }
  bool                is_active()      const { return state() == STATE_ACTIVE; }
  bool                is_inactive()    const { return state() == STATE_INACTIVE; }

  bool                is_polling() const;
  bool                is_current() const;

  bool                has_no_timeout()   const { return (flags() & flag_no_timeout); }
  bool                has_do_shutdown()  const { return (flags() & flag_do_shutdown); }
  bool                has_did_shutdown() const { return (flags() & flag_did_shutdown); }

  state_type          state() const { return m_state; }
  int                 flags() const { return m_flags; }

  virtual const char* name() const = 0;

  Poll*               poll()            { return m_poll; }
  signal_bitfield_t*  signal_bitfield() { return &m_signal_bitfield; }
  pthread_t           pthread()         { return m_thread; }
  std::thread::id     thread_id()       { return m_thread_id; }

  virtual void        init_thread() = 0;

  virtual void        start_thread();
  virtual void        stop_thread();
  void                stop_thread_wait();

  void                interrupt();
  void                send_event_signal(unsigned int index, bool interrupt = true);

  slot_void&          slot_do_work()      { return m_slot_do_work; }
  slot_timer&         slot_next_timeout() { return m_slot_next_timeout; }

  static inline int   global_queue_size() { return m_global.waiting; }

  static inline void  acquire_global_lock();
  static inline bool  trylock_global_lock();
  static inline void  release_global_lock();
  static inline void  waive_global_lock();

  static bool         should_handle_sigusr1();

  static void*        event_loop(Thread* thread);

protected:
  struct global_lock_type {
    std::atomic_int waiting{0};
    std::mutex      mutex;
  };

  virtual void        call_events() = 0;
  virtual int64_t     next_timeout_usec() = 0;

  static global_lock_type m_global;

  pthread_t                    m_thread;
  std::atomic<std::thread::id> m_thread_id;
  std::atomic<state_type>      m_state{STATE_UNKNOWN};
  std::atomic_int              m_flags{0};

  int                 m_instrumentation_index;

  Poll*               m_poll{nullptr};
  signal_bitfield_t   m_signal_bitfield;

  slot_void           m_slot_do_work;
  slot_timer          m_slot_next_timeout;

  std::unique_ptr<thread_interrupt> m_interrupt_sender;
  std::unique_ptr<thread_interrupt> m_interrupt_receiver;
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

inline void
Thread::acquire_global_lock() {
  Thread::m_global.waiting++;
  Thread::m_global.mutex.lock();
  Thread::m_global.waiting--;
}

inline bool
Thread::trylock_global_lock() {
  return Thread::m_global.mutex.try_lock();
}

inline void
Thread::release_global_lock() {
  Thread::m_global.mutex.unlock();
}

inline void
Thread::waive_global_lock() {
  release_global_lock();
  acquire_global_lock();
}

}

#endif
