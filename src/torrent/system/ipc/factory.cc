#include "config.h"

#include "torrent/system/ipc/factory.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/system/types.h"
#include "torrent/system/ipc/channel.h"
#include "torrent/system/ipc/router.h"
#include "torrent/system/ipc/segment.h"
#include "torrent/system/ipc/wakeup_fd.h"

namespace torrent::system::ipc {

namespace {

std::pair<int, int>
create_socket_pair() {
  auto setup_socket = [](int fd) {
      if (!fd_set_send_timeout(fd, 2s))
        throw internal_error("ControlFd::send_fatal_error(): fd_set_send_timeout() failed: " + errno_enum_str(errno));

      if (!fd_set_receive_timeout(fd, 2s))
        throw internal_error("ControlFd::initialize(): fd_set_receive_timeout() failed: " + errno_enum_str(errno));

      // Linux kernels require O_NONBLOCK alongside SO_SNDTIMEO to respect timeouts on AF_LOCAL.
      // macOS and BSD require the socket to remain blocking for the timeout to function.

#ifdef __linux__
      int flags = ::fcntl(fd, F_GETFL, 0);

      if (flags == -1 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw internal_error("RouterFactory::initialize(): fcntl(O_NONBLOCK) failed: " + errno_enum_str(errno));

#endif
    };

  int fd_0;
  int fd_1;

  fd_open_socket_pair(fd_0, fd_1);

  setup_socket(fd_0);
  setup_socket(fd_1);

  return {fd_0, fd_1};
}

} // namespace anonymous


void
RouterFactory::initialize(uint32_t segment_size) {
  m_segment_1 = std::make_unique<Segment>();
  m_segment_2 = std::make_unique<Segment>();

  m_segment_1->create(segment_size);
  m_segment_2->create(segment_size);

  static_cast<torrent::system::ipc::Channel*>(m_segment_1->address())->initialize(m_segment_1->address(), m_segment_1->size());
  static_cast<torrent::system::ipc::Channel*>(m_segment_2->address())->initialize(m_segment_2->address(), m_segment_2->size());

  m_control_fds   = create_socket_pair();
  m_keepalive_fds = create_socket_pair();

  m_wakeup_fds = WakeupFd::create_fd_pair();
}

// TODO: Use unique_ptr in Router for Segments, and let it steal our ptrs.

// TODO: When adding EventFd/kqueue-event for wakeup, move control-fd listening to a separate thread and remove keepalife_fd.

std::unique_ptr<Router>
RouterFactory::create_parent_router() {
  ::close(m_control_fds.second);
  ::close(m_keepalive_fds.second);

  if (!fd_set_close_on_exec(m_control_fds.first, true))
    throw internal_error("RouterFactory::create_parent_router(): fd_set_close_on_exec() failed: " + errno_enum_str(errno));

  if (!fd_set_close_on_exec(m_keepalive_fds.first, true))
    throw internal_error("RouterFactory::create_parent_router(): fd_set_close_on_exec() failed: " + errno_enum_str(errno));

  // return std::make_unique<Router>(m_control_fd.first, m_keepalive_fd.first, std::move(m_segment_1), std::move(m_segment_2));

  return std::make_unique<Router>(Router::create_args{
      true,
      m_control_fds.first,
      m_keepalive_fds.first,
      m_wakeup_fds,
      std::move(m_segment_1),
      std::move(m_segment_2)
    });
}

std::unique_ptr<Router>
RouterFactory::create_child_router() {
  ::close(m_control_fds.first);
  ::close(m_keepalive_fds.first);

  if (!fd_set_close_on_exec(m_control_fds.second, true))
    throw internal_error("RouterFactory::create_child_router(): fd_set_close_on_exec() failed: " + errno_enum_str(errno));

  if (!fd_set_close_on_exec(m_keepalive_fds.second, true))
    throw internal_error("RouterFactory::create_child_router(): fd_set_close_on_exec() failed: " + errno_enum_str(errno));

  // Router::create_args args;

  return std::make_unique<Router>(Router::create_args{
      false,
      m_control_fds.second,
      m_keepalive_fds.second,
      m_wakeup_fds,
      std::move(m_segment_2),
      std::move(m_segment_1)
    });
}

} // namespace torrent::system::ipc
