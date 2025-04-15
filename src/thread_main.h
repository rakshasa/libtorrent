#ifndef LIBTORRENT_THREAD_MAIN_H
#define LIBTORRENT_THREAD_MAIN_H

#include <memory>

#include "torrent/common.h"
#include "torrent/utils/thread.h"

namespace torrent {

class HashQueue;

class LIBTORRENT_EXPORT ThreadMain : public utils::Thread {
public:

  static void         create_thread();
  static ThreadMain*  thread_main();

  const char*         name() const override { return "rtorrent main"; }

  void                init_thread() override;

  HashQueue*          hash_queue()          { return m_hash_queue.get(); }

  auto&               slot_do_work()        { return m_slot_do_work; }
  auto&               slot_next_timeout()   { return m_slot_next_timeout; }

protected:
  ThreadMain() = default;
  ~ThreadMain() override;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

  static ThreadMain*         m_thread_main;

  std::unique_ptr<HashQueue> m_hash_queue;

  std::function<void()>      m_slot_do_work;
  std::function<uint64_t()>  m_slot_next_timeout;
};

inline ThreadMain* thread_main() {
  return ThreadMain::thread_main();
}

} // namespace torrent

#endif // LIBTORRENT_THREAD_MAIN_H
