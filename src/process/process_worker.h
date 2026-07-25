#ifndef LIBTORRENT_PROCESS_PROCESS_WORKER_H
#define LIBTORRENT_PROCESS_PROCESS_WORKER_H

#include <torrent/system/common.h>

namespace torrent::process {

class ProcessWorker {
public:
  ProcessWorker();
  ~ProcessWorker();

  auto&               router();

  void                spawn();

private:
  ProcessWorker(const ProcessWorker&) = delete;
  ProcessWorker& operator=(const ProcessWorker&) = delete;

  system::ipc_router_ptr m_router;
};

inline auto& ProcessWorker::router() { return m_router; }

} // namespace torrent::process

#endif
