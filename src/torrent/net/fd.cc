#include "config.h"

#include "fd.h"

#include <cerrno>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

void
fd_close(int fd) {
  if (::close(fd) == -1)
    throw internal_error("torrent::fd_close(...) failed: " + std::string(strerror(errno)));
}

bool
fd_bind(int fd, const sockaddr* sa, bool use_inet6) {
  if (use_inet6 && sa->sa_family == AF_INET) {
    sockaddr_in6 mapped;
    sa_inet_mapped_inet6(reinterpret_cast<const sockaddr_in*>(sa), &mapped);

    return ::bind(fd, reinterpret_cast<sockaddr*>(&mapped), sizeof(sockaddr_in6)) == 0;
  }

  return ::bind(fd, sa, sa_length(sa)) == 0;
}

}
