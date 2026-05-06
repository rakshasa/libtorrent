#ifndef LIBTORRENT_THREAD_MAIN_H
#define LIBTORRENT_THREAD_MAIN_H

#include <memory>

#include "torrent/common.h"
#include "torrent/system/thread.h"

namespace torrent {

namespace tracker {

class UdpRouter;

} // namespace tracker

class HashQueue;

class LIBTORRENT_EXPORT ThreadMain : public system::Thread {
public:
  ~ThreadMain() override;

  static void         create_thread();
  static ThreadMain*  thread_main();

  const char*         name() const override { return "rtorrent main"; }

  void                init_thread() override;
  void                init_thread_post_local() override;
  void                cleanup_thread() override;

  HashQueue*          hash_queue()          { return m_hash_queue.get(); }

  auto&               slot_do_work()        { return m_slot_do_work; }

  auto                  udp_inet_router()         { return m_udp_inet_router.get(); }
  auto                  udp_inet6_router()        { return m_udp_inet6_router.get(); }

protected:
  friend class ThreadMainInternal;

  ThreadMain() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  static ThreadMain*         m_thread_main;

  std::unique_ptr<HashQueue> m_hash_queue;
  std::function<void()>      m_slot_do_work;

  std::unique_ptr<tracker::UdpRouter> m_udp_inet_router;
  std::unique_ptr<tracker::UdpRouter> m_udp_inet6_router;
};

} // namespace torrent

#endif // LIBTORRENT_THREAD_MAIN_H
