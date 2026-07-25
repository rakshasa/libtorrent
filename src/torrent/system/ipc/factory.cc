#include "config.h"

#include "torrent/system/ipc/factory.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/system/ipc/channel.h"
#include "torrent/system/ipc/router.h"
#include "torrent/system/ipc/segment.h"

namespace torrent::system::ipc {

void
RouterFactory::initialize(uint32_t segment_size) {
  m_segment_1 = std::make_unique<Segment>();
  m_segment_2 = std::make_unique<Segment>();

  m_segment_1->create(segment_size);
  m_segment_2->create(segment_size);

  static_cast<torrent::system::ipc::Channel*>(m_segment_1->address())->initialize(m_segment_1->address(), m_segment_1->size());
  static_cast<torrent::system::ipc::Channel*>(m_segment_2->address())->initialize(m_segment_2->address(), m_segment_2->size());

  int socket_pair[2]{};

  if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, socket_pair) == -1)
    throw internal_error("RouterFactory::initialize(): socketpair() failed: " + std::string(strerror(errno)));

  // Use timeouts as the control channel should never be able to exhaust buffers.
  //
  // Sockets remain blocking on macOS/BSD, but use O_NONBLOCK loops on Linux to respect timeouts.

  auto setup_socket = [](int fd) {
      struct timeval timeout{2, 0};

      if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1)
        throw internal_error("ControlFd::send_fatal_error(): setsockopt(SO_SNDTIMEO) failed: " + std::string(std::strerror(errno)));

      if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
        throw internal_error("ControlFd::initialize(): setsockopt(SO_RCVTIMEO) failed: " + std::string(strerror(errno)));

      // Linux kernels require O_NONBLOCK alongside SO_SNDTIMEO to respect timeouts on AF_LOCAL.
      // macOS and BSD require the socket to remain blocking for the timeout to function.

#ifdef __linux__
      int flags = ::fcntl(fd, F_GETFL, 0);

      if (flags == -1 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw internal_error("RouterFactory::initialize(): fcntl(O_NONBLOCK) failed: " + std::string(strerror(errno)));

#endif
    };

  setup_socket(socket_pair[0]);
  setup_socket(socket_pair[1]);

  m_socket_1 = socket_pair[0];
  m_socket_2 = socket_pair[1];
}

// TODO: Use unique_ptr in Router, and let it steal our ptrs.

std::unique_ptr<Router>
RouterFactory::create_parent_router() {
  ::close(m_socket_2);

  return std::make_unique<Router>(m_socket_1, std::move(m_segment_1), std::move(m_segment_2));
}

std::unique_ptr<Router>
RouterFactory::create_child_router() {
  ::close(m_socket_1);

  return std::make_unique<Router>(m_socket_2, std::move(m_segment_2), std::move(m_segment_1));
}

} // namespace torrent::system::ipc
