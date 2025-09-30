#include "config.h"

#include "socket_fd.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "torrent/exceptions.h"

namespace torrent {

inline void
SocketFd::check_valid() const {
  if (!is_valid())
    throw internal_error("SocketFd function called on an invalid fd.");
}

bool
SocketFd::set_nonblock() {
  check_valid();

  return fcntl(m_fd, F_SETFL, O_NONBLOCK) == 0;
}

bool
SocketFd::set_reuse_address(bool state) {
  check_valid();
  int opt = state;

  return setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

bool
SocketFd::set_ipv6_v6only(bool state) {
  check_valid();

  if (!m_ipv6_socket)
    return false;

  int opt = state;
  return setsockopt(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == 0;
}

bool
SocketFd::open_stream() {
  m_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

  if (m_fd == -1) {
    m_ipv6_socket = false;
    return (m_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) != -1;
  }

  m_ipv6_socket = true;

  if (!set_ipv6_v6only(false)) {
    close();
    return false;
  }

  return true;
}

bool
SocketFd::open_datagram() {
  m_fd = socket(AF_INET6, SOCK_DGRAM, 0);

  if (m_fd == -1) {
    m_ipv6_socket = false;
    return (m_fd = socket(AF_INET, SOCK_DGRAM, 0)) != -1;
  }

  m_ipv6_socket = true;

  if (!set_ipv6_v6only(false)) {
    close();
    return false;
  }

  return true;
}

bool
SocketFd::open_local() {
  return (m_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) != -1;
}

bool
SocketFd::open_socket_pair(int& fd1, int& fd2) {
  int result[2];

  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, result) == -1)
    return false;

  fd1 = result[0];
  fd2 = result[1];

  return true;
}

void
SocketFd::close() const {
  if (::close(m_fd) && errno == EBADF)
    throw internal_error("SocketFd::close() called on an invalid file descriptor");
}

} // namespace torrent
