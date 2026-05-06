#ifndef LIBTORRENT_THREAD_TRACKER_H
#define LIBTORRENT_THREAD_TRACKER_H

#include "torrent/common.h"
#include "torrent/system/thread.h"

namespace torrent {

namespace tracker {

class Manager;
class UdpRouter;

} // namespace tracker

class LIBTORRENT_EXPORT ThreadTracker : public system::Thread {
public:
  ~ThreadTracker() override;

  static void           create_thread(system::Thread* main_thread);
  static void           destroy_thread();
  static ThreadTracker* thread_tracker();

  const char*           name() const override     { return "rtorrent tracker"; }

  void                  init_thread() override;
  void                  init_thread_post_local() override;
  void                  cleanup_thread() override;

  // TODO: Make protected?

  tracker::Manager*     tracker_manager()         { return m_tracker_manager.get(); }

  // auto                  udp_inet_router()         { return m_udp_inet_router.get(); }
  // auto                  udp_inet6_router()        { return m_udp_inet6_router.get(); }

protected:
  friend class Manager;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

private:
  ThreadTracker() = default;

  static ThreadTracker*               m_thread_tracker;

  std::unique_ptr<tracker::Manager>   m_tracker_manager;
  // std::unique_ptr<tracker::UdpRouter> m_udp_inet_router;
  // std::unique_ptr<tracker::UdpRouter> m_udp_inet6_router;
};

} // namespace torrent

#endif // LIBTORRENT_THREAD_TRACKER_H
