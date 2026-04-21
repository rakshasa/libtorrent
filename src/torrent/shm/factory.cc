#include "config.h"

#include "torrent/shm/factory.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/shm/channel.h"
#include "torrent/shm/router.h"
#include "torrent/shm/segment.h"

namespace torrent::shm {

void
RouterFactory::initialize(uint32_t segment_size) {
  m_segment_1 = std::make_unique<Segment>();
  m_segment_2 = std::make_unique<Segment>();

  m_segment_1->create(segment_size);
  m_segment_2->create(segment_size);

  static_cast<torrent::shm::Channel*>(m_segment_1->address())->initialize(m_segment_1->address(), m_segment_1->size());
  static_cast<torrent::shm::Channel*>(m_segment_2->address())->initialize(m_segment_2->address(), m_segment_2->size());

  // TODO: Use fd_*()

  int socket_pair[2]{};

  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, socket_pair) == -1)
    throw internal_error("RouterFactory::initialize(): socketpair() failed: " + std::string(strerror(errno)));

  if (fcntl(socket_pair[0], F_SETFL, O_NONBLOCK) == -1)
    throw internal_error("RouterFactory::initialize(): fcntl() failed: " + std::string(strerror(errno)));

  if (fcntl(socket_pair[1], F_SETFL, O_NONBLOCK) == -1)
    throw internal_error("RouterFactory::initialize(): fcntl() failed: " + std::string(strerror(errno)));

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

} // namespace torrent::shm
