#ifndef LIBTORRENT_UTILS_THREAD_INTERNAL_H
#define LIBTORRENT_UTILS_THREAD_INTERNAL_H

#include "torrent/common.h"
#include "torrent/system/thread.h"

namespace torrent::system {

class ThreadInternal {
public:
  static auto*                     thread()         { return Thread::m_self; }
  static auto                      thread_id()      { return Thread::m_self->thread_id(); }

  static std::chrono::microseconds cached_time()    { return Thread::m_self->m_cached_time; }
  static std::chrono::seconds      cached_seconds() { return utils::cast_seconds(Thread::m_self->m_cached_time); }

  static system::Poll*             poll()           { return Thread::m_self->m_poll.get(); }
  static auto*                     scheduler()      { return Thread::m_self->m_scheduler.get(); }
  static net::Resolver*            resolver()       { return Thread::m_self->m_resolver.get(); }
};

} // namespace torrent::utils

#endif // LIBTORRENT_UTILS_THREAD_INTERNAL_H
