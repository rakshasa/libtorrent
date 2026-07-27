#ifndef LIBTORRENT_PROCESS_THREAD_WORKER_H
#define LIBTORRENT_PROCESS_THREAD_WORKER_H

#include "torrent/common.h"
#include "torrent/system/thread.h"

namespace torrent::process::worker {

class ThreadWorker : public system::Thread {
public:
  ~ThreadWorker() override;

  static void            create_thread();

  static ThreadWorker*   thread_worker();
  static system::Thread* thread_base();

  const char*            name() const override { return "rtorrent-worker"; }

  void                   init_thread() override;
  void                   init_after_setup();
  void                   cleanup_thread() override;

protected:
  ThreadWorker() = default;

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;

private:
  static void               set_max_connections();

  static ThreadWorker*      m_thread_worker;
};

inline ThreadWorker*   ThreadWorker::thread_worker() { return m_thread_worker; }
inline system::Thread* ThreadWorker::thread_base()   { return m_thread_worker; }

} // namespace torrent::process::worker

#endif
