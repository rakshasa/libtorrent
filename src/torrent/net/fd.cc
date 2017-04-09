#include "config.h"

#include "fd.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

int
fd_open(fd_flags flags) {
  int domain;
  int protocol;

  if ((flags & fd_flag_stream)) {
    domain = SOCK_STREAM;
    protocol = IPPROTO_TCP;
  } else {
    errno = EINVAL;
    return -1;
  }

  int fd = -1;
  bool ipv6 = false;

  if (fd == -1 && !(flags & fd_flag_v4only)) {
    fd = socket(PF_INET6, domain, protocol);
    ipv6 = true;
  }

  if (fd == -1 && !(flags & fd_flag_v6only)) {
    fd = socket(PF_INET, domain, protocol);
    ipv6 = false;
  }

  if (fd == -1)
    return -1;

  // TODO: Remove ipv6 check?
  if ((flags & fd_flag_v6only) && ipv6 && !fd_set_v6only(fd, true)) {
    fd_close(fd);
    return -1;
  }

  if ((flags & fd_flag_nonblock) && !fd_set_nonblock(fd)) {
    fd_close(fd);
    return -1;
  }

  if ((flags & fd_flag_reuse_address) && !fd_set_reuse_address(fd, true)) {
    fd_close(fd);
    return -1;
  }

  return fd;
}

void
fd_close(int fd) {
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
    throw internal_error("torrent::fd_close(...) failed: tried to close stdin/out/err");

  if (::close(fd) == -1)
    throw internal_error("torrent::fd_close(...) failed: " + std::string(strerror(errno)));
}

bool
fd_set_nonblock(int fd) {
  return fcntl(fd, F_SETFL, O_NONBLOCK) == 0;
}

bool
fd_set_reuse_address(int fd, bool state) {
  int opt = state;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

bool
fd_set_v6only(int fd, bool state) {
  int opt = state;
  return setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == 0;
}

bool
fd_connect(int fd, const sockaddr* sa) {
  return ::connect(fd, sa, sa_length(sa)) == 0 || errno == EINPROGRESS;
}

bool
fd_bind(int fd, const sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    sockaddr_in6 mapped;
    sa_inet_mapped_inet6(reinterpret_cast<const sockaddr_in*>(sa), &mapped);

    return ::bind(fd, reinterpret_cast<sockaddr*>(&mapped), sizeof(sockaddr_in6)) == 0;
  }

  return ::bind(fd, sa, sa_length(sa)) == 0;
}

bool
fd_listen(int fd, int backlog) {
  return ::listen(fd, backlog) == 0;
}

}
