#include "config.h"

#include "torrent/utils/process.h"

#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/shm/channel.h"
#include "torrent/shm/router.h"
#include "torrent/shm/segment.h"

// TODO: Move to 'torrent/system'

namespace torrent::utils {

Process::Process() {
}

Process::~Process() = default;

void
Process::start_process() {
  auto segment_1 = std::make_unique<torrent::shm::Segment>();
  auto segment_2 = std::make_unique<torrent::shm::Segment>();

  segment_1->create(1 * torrent::shm::Segment::page_size);
  segment_2->create(1 * torrent::shm::Segment::page_size);

  auto channel_1 = static_cast<torrent::shm::Channel*>(segment_1->address());
  auto channel_2 = static_cast<torrent::shm::Channel*>(segment_2->address());

  channel_1->initialize(segment_1->address(), segment_1->size());
  channel_2->initialize(segment_2->address(), segment_2->size());

  int child_fd, parent_fd;

  fd_open_socket_pair(child_fd, parent_fd);

  if (!fd_set_nonblock(child_fd))
    throw torrent::resource_error("Failed to set non-blocking mode on child socket: " + std::string(std::strerror(errno)));

  if (!fd_set_nonblock(parent_fd))
    throw torrent::resource_error("Failed to set non-blocking mode on parent socket: " + std::string(std::strerror(errno)));

  pid_t pid = fork();

  if (pid == -1)
    throw torrent::resource_error("Failed to fork process: " + std::string(std::strerror(errno)));

  if (pid == 0) {
    m_pid           = getpid();
    m_read_router   = std::make_unique<torrent::shm::Router>(child_fd, channel_2, channel_1);
    m_write_router  = std::make_unique<torrent::shm::Router>(child_fd, channel_1, channel_2);
    m_read_segment  = std::move(segment_2);
    m_write_segment = std::move(segment_1);

    sleep(1000000);

    throw torrent::internal_error("Process::start_process() child process should not return from this function");
  }

  m_pid           = pid;
  m_read_router   = std::make_unique<torrent::shm::Router>(parent_fd, channel_1, channel_2);
  m_write_router  = std::make_unique<torrent::shm::Router>(parent_fd, channel_2, channel_1);
  m_read_segment  = std::move(segment_1);
  m_write_segment = std::move(segment_2);
}

} // namespace torrent::utils
