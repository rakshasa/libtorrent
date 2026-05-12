#ifndef LIBTORRENT_THREAD_MAIN_H
#define LIBTORRENT_THREAD_MAIN_H

#include <memory>

#include "torrent/common.h"
#include "torrent/system/thread.h"

class TestMainThread;

namespace torrent {

class HashQueue;

class LIBTORRENT_EXPORT ThreadMain : public system::Thread {
public:
  ~ThreadMain() override;

  static void            create_thread();
  static ThreadMain*     thread_main()         { return m_thread_main; }
  static system::Thread* thread_base()         { return m_thread_base; }

  const char*            name() const override { return "rtorrent-main"; }

  void                   init_thread() override;
  void                   cleanup_thread() override;

  HashQueue*             hash_queue()          { return m_hash_queue.get(); }

  auto&                  slot_do_work()        { return m_slot_do_work; }

protected:
  friend class ::TestMainThread;

  ThreadMain() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  static void               set_thread_base(system::Thread* thread) { m_thread_base = thread; }

private:
  static ThreadMain*         m_thread_main;
  static system::Thread*     m_thread_base;

  std::unique_ptr<HashQueue> m_hash_queue;
  std::function<void()>      m_slot_do_work;
};

} // namespace torrent

#endif // LIBTORRENT_THREAD_MAIN_H
