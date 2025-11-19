#ifndef LIBTORRENT_UTILS_THREAD_INTERNAL_H
#define LIBTORRENT_UTILS_THREAD_INTERNAL_H

#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent::utils {

class ThreadInternal {
public:
  static torrent::utils::Thread*   thread()         { return Thread::m_self; }
  static std::thread::id           thread_id()      { return Thread::m_self->thread_id(); }

  static std::chrono::microseconds cached_time()    { return Thread::m_self->m_cached_time; }
  static std::chrono::seconds      cached_seconds() { return cast_seconds(Thread::m_self->m_cached_time); }

  static void                      callback(void* target, std::function<void ()>&& fn) { Thread::m_self->callback(target, std::move(fn)); }
  static void                      cancel_callback(void* target)                       { Thread::m_self->cancel_callback(target); }
  static void                      cancel_callback_and_wait(void* target)              { Thread::m_self->cancel_callback_and_wait(target); }

  static net::Poll*                poll()           { return Thread::m_self->m_poll.get(); }
  static Scheduler*                scheduler()      { return Thread::m_self->m_scheduler.get(); }
  static net::Resolver*            resolver()       { return Thread::m_self->m_resolver.get(); }
};

} // namespace torrent::utils

#endif // LIBTORRENT_UTILS_THREAD_INTERNAL_H
