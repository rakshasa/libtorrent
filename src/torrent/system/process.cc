#include "config.h"

#include "torrent/system/process.h"

#include <cstdlib>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/shm/channel.h"
#include "torrent/shm/factory.h"
#include "torrent/shm/router.h"
#include "torrent/shm/segment.h"
#include "torrent/utils/log.h"

// #define LT_LOG(log_fmt, ...)                                            \
//   lt_log_print(LOG_SYSTEM, "system: " log_fmt, __VA_ARGS__);

namespace torrent::system {

Process::Process() = default;
Process::~Process() = default;

void
Process::start(std::function<void()> fn) {
  if (m_router_factory)
    throw internal_error("Process::start_process() process already started");

  // TODO: Figure out the best size, however use a small size to test channel congestion handling.
  m_router_factory = std::make_unique<shm::RouterFactory>();
  m_router_factory->initialize(1 * shm::Segment::page_size);

  pid_t pid = fork();

  if (pid == -1)
    throw internal_error("Process::start_process() fork() failed: " + std::string(strerror(errno)));

  m_pid = pid;

  if (pid == 0) {
    // Child process, nothing else to do here, just return and let the caller handle it.
    m_router = m_router_factory->create_child_router();

    try {
      fn();

    } catch (...) {
      // TODO: We need to send the error message to the parent process through the router.
      throw;
    }

    std::exit(0);

  } else {
    // Parent process, nothing else to do here, just return and let the caller handle it.
    m_router = m_router_factory->create_parent_router();
  }
}

} // namespace torrent::utils
