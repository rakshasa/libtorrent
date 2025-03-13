#ifndef LIBTORRENT_THREAD_MAIN_H
#define LIBTORRENT_THREAD_MAIN_H

#include <memory>

#include "torrent/utils/thread.h"

namespace torrent {

class HashQueue;

class ThreadMain : public utils::Thread {
public:
  ThreadMain();

  const char*         name() const override  { return "rtorrent main"; }

  void                init_thread() override;

  HashQueue*          hash_queue()           { return m_hash_queue.get(); }

protected:
  void                call_events() override;
  int64_t             next_timeout_usec() override;

  std::unique_ptr<HashQueue> m_hash_queue;
};

extern ThreadMain* thread_main;

}

#endif
