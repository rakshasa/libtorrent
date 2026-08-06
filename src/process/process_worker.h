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

  void                init_parent_process();

private:
  ProcessWorker(const ProcessWorker&) = delete;
  ProcessWorker& operator=(const ProcessWorker&) = delete;

  void                init_child_process();

  system::ipc_router_ptr m_router;

  std::unique_ptr<system::ipc::RouterFactory> m_factory;
};

inline auto& ProcessWorker::router() { return m_router; }

} // namespace torrent::process

#endif
