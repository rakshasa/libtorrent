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

  const char*         name() const override  { return "rtorrent main"; }

  void                init_thread() override;

  HashQueue*          hash_queue()           { return m_hash_queue.get(); }

protected:
  ThreadMain() = default;
  ~ThreadMain() override;

  void                call_events() override;
  int64_t             next_timeout_usec() override;

  static ThreadMain*         m_thread_main;

  std::unique_ptr<HashQueue> m_hash_queue;
};

inline ThreadMain* thread_main() {
  return ThreadMain::thread_main();
}

} // namespace torrent

#endif // LIBTORRENT_THREAD_MAIN_H
