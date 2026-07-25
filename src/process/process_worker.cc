#include "config.h"

#include "process_worker.h"

#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/system/ipc/factory.h"

namespace torrent::process {

ProcessWorker::ProcessWorker() = default;
ProcessWorker::~ProcessWorker() = default;

void
ProcessWorker::spawn() {
  system::ipc::RouterFactory factory;

  factory.initialize(1 * 4096);

  pid_t pid = ::fork();

  if (pid == -1)
    throw internal_error("fork() failed: " + std::string(strerror(errno)));

  if (pid != 0) {
    // TODO: Add pid to sig handler to kill child process on crash.

    m_router = factory.create_parent_router();
    return;
  }

  m_router = factory.create_child_router();

  ::sleep(180);
}

} // namespace torrent::process
