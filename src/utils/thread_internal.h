#ifndef LIBTORRENT_UTILS_THREAD_INTERNAL_H
#define LIBTORRENT_UTILS_THREAD_INTERNAL_H

#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent::utils {

class ThreadInternal {
public:
  static std::chrono::microseconds cached_time()    { return Thread::m_self->m_cached_time; }
  static std::chrono::seconds      cached_seconds() { return cast_seconds(Thread::m_self->m_cached_time); }
  static Poll*                     poll()           { return Thread::m_self->m_poll.get(); }
  static Scheduler*                scheduler()      { return Thread::m_self->m_scheduler.get(); }
  static net::Resolver*            resolver()       { return Thread::m_self->m_resolver.get(); }
};

} // namespace torrent::utils

#endif // LIBTORRENT_UTILS_THREAD_INTERNAL_H
