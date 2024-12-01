#ifndef LIBTORRENT_UTILS_THREAD_BASE_H
#define LIBTORRENT_UTILS_THREAD_BASE_H

#import <functional>
#import <mutex>
#import <pthread.h>
#import <sys/types.h>

#import <torrent/common.h>
#import <torrent/utils/signal_bitfield.h>

namespace torrent {

class Poll;
class thread_interrupt;

class LIBTORRENT_EXPORT lt_cacheline_aligned thread_base {
public:
  typedef void* (*pthread_func)(void*);
  typedef std::function<void ()>     slot_void;
  typedef std::function<uint64_t ()> slot_timer;
  typedef class signal_bitfield      signal_type;

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

  thread_base();
  virtual ~thread_base();

  bool                is_initialized() const { return state() == STATE_INITIALIZED; }
  bool                is_active()      const { return state() == STATE_ACTIVE; }
  bool                is_inactive()    const { return state() == STATE_INACTIVE; }

  bool                is_polling() const;
  bool                is_current() const;

  bool                has_no_timeout()   const { return (flags() & flag_no_timeout); }
  bool                has_do_shutdown()  const { return (flags() & flag_do_shutdown); }
  bool                has_did_shutdown() const { return (flags() & flag_did_shutdown); }

  state_type          state() const;
  int                 flags() const;

  virtual const char* name() const = 0;

  Poll*               poll()            { return m_poll; }
  signal_type*        signal_bitfield() { return &m_signal_bitfield; }
  pthread_t           pthread()         { return m_thread; }

  virtual void        init_thread() = 0;

  virtual void        start_thread();
  virtual void        stop_thread();
  void                stop_thread_wait();

  void                interrupt();
  void                send_event_signal(unsigned int index, bool interrupt = true);

  slot_void&          slot_do_work()      { return m_slot_do_work; }
  slot_timer&         slot_next_timeout() { return m_slot_next_timeout; }

  static inline int   global_queue_size() { return m_waiting; }

  static inline void  acquire_global_lock();
  static inline bool  trylock_global_lock();
  static inline void  release_global_lock();
  static inline void  waive_global_lock();

  static bool         should_handle_sigusr1();

  static void*        event_loop(thread_base* thread);

protected:
  static std::mutex lt_cacheline_aligned m_globalLock;
  static std::atomic<int> lt_cacheline_aligned m_waiting;

  virtual void        call_events() = 0;
  virtual int64_t     next_timeout_usec() = 0;


  pthread_t           m_thread;

  std::atomic<state_type>     lt_cacheline_aligned     m_state{STATE_UNKNOWN};
  std::atomic<int>            lt_cacheline_aligned     m_flags{0};

  int                 m_instrumentation_index;

  Poll*               m_poll;
  signal_type         m_signal_bitfield;

  slot_void           m_slot_do_work;
  slot_timer          m_slot_next_timeout;

  thread_interrupt*   m_interrupt_sender;
  thread_interrupt*   m_interrupt_receiver;
};

inline bool
thread_base::is_polling() const {
  return (flags() & flag_polling);
}

inline bool
thread_base::is_current() const {
  return m_thread == pthread_self();
}

inline int
thread_base::flags() const {
  return m_flags;
}

inline thread_base::state_type
thread_base::state() const {
  return m_state;
}

inline void
thread_base::send_event_signal(unsigned int index, bool do_interrupt) {
  m_signal_bitfield.signal(index);

  if (do_interrupt)
    interrupt();
}

inline void
thread_base::acquire_global_lock() {
  ++m_waiting;
  m_globalLock.lock();
  --m_waiting;
}

inline bool
thread_base::trylock_global_lock() {
  return thread_base::m_globalLock.try_lock();
}

inline void
thread_base::release_global_lock() {
  return thread_base::m_globalLock.unlock();
}

inline void
thread_base::waive_global_lock() {
  thread_base::m_globalLock.unlock();

  // Do we need to sleep here? Make a CppUnit test for this.
  acquire_global_lock();
}

}  

#endif
