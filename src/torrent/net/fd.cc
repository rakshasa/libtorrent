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
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                    \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt, __VA_ARGS__);
#define LT_LOG_FLAG(log_fmt)                                            \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " (flags:0x%x)", flags);
#define LT_LOG_FLAG_ERROR(log_fmt)                                      \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " (flags:0x%x errno:%i message:'%s')", \
               flags, errno, std::strerror(errno));
#define LT_LOG_FD(log_fmt)                                      \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt, fd);
#define LT_LOG_FD_ERROR(log_fmt)                                        \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (errno:%i message:'%s')", \
               fd, errno, std::strerror(errno));
#define LT_LOG_FD_SOCKADDR(log_fmt)                                   \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s)", \
               fd, sa_pretty_str(sa).c_str());
#define LT_LOG_FD_SOCKADDR_ERROR(log_fmt)                               \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s errno:%i message:'%s')", \
               fd, sa_pretty_str(sa).c_str(), errno, std::strerror(errno));
#define LT_LOG_FD_FLAG(log_fmt)                                         \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (flags:0x%x)", fd, flags);
#define LT_LOG_FD_FLAG_ERROR(log_fmt)                                   \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (flags:0x%x errno:%i message:'%s')", \
               fd, flags, errno, std::strerror(errno));
#define LT_LOG_FD_VALUE(log_fmt, value)                                 \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (value:%i)", fd, (int)value);
#define LT_LOG_FD_VALUE_ERROR(log_fmt, value)                           \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (value:%i errno:%i message:'%s')", \
               fd, (int)value, errno, std::strerror(errno));

namespace torrent {

int
fd_open(fd_flags flags) {
  int domain;
  int protocol;

  if ((flags & fd_flag_stream)) {
    domain = SOCK_STREAM;
    protocol = IPPROTO_TCP;
  } else {
    LT_LOG_FLAG("fd_open missing socket type");
    errno = EINVAL;
    return -1;
  }

  int fd = -1;
  bool ipv6 = false;

  if (fd == -1 && !(flags & fd_flag_v4only)) {
    LT_LOG_FLAG("fd_open opening ipv6 socket");
    fd = socket(PF_INET6, domain, protocol);
    ipv6 = true;
  }

  if (fd == -1 && !(flags & fd_flag_v6only)) {
    LT_LOG_FLAG("fd_open opening ipv4 socket");
    fd = socket(PF_INET, domain, protocol);
    ipv6 = false;
  }

  if (fd == -1) {
    LT_LOG_FLAG_ERROR("fd_open failed to open socket");
    return -1;
  }

  // TODO: Remove ipv6 check? Or rather, error if inet.
  if ((flags & fd_flag_v6only) && ipv6 && !fd_set_v6only(fd, true)) {
    LT_LOG_FD_FLAG_ERROR("fd_open failed to set v6only");
    fd_close(fd);
    return -1;
  }

  if ((flags & fd_flag_nonblock) && !fd_set_nonblock(fd)) {
    LT_LOG_FD_FLAG_ERROR("fd_open failed to set nonblock");
    fd_close(fd);
    return -1;
  }

  if ((flags & fd_flag_reuse_address) && !fd_set_reuse_address(fd, true)) {
    LT_LOG_FD_FLAG_ERROR("fd_open failed to set reuse_address");
    fd_close(fd);
    return -1;
  }

  LT_LOG_FD_FLAG("fd_open succeeded");
  return fd;
}

void
fd_close(int fd) {
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
    throw internal_error("torrent::fd_close(...) failed: tried to close stdin/out/err");

  if (::close(fd) == -1)
    throw internal_error("torrent::fd_close(...) failed: " + std::string(strerror(errno)));

  LT_LOG_FD("fd_close succeeded");
}

bool
fd_set_nonblock(int fd) {
  if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
    LT_LOG_FD_ERROR("fd_set_nonblock failed");
    return false;
  }

  LT_LOG_FD("fd_set_nonblock succeeded");
  return true;
}

bool
fd_set_reuse_address(int fd, bool state) {
  int opt = state;

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_reuse_address failed", opt);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_reuse_address succeeded", opt);
  return true;
}

bool
fd_set_v6only(int fd, bool state) {
  int opt = state;

  if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_v6only failed", opt);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_v6only succeeded", opt);
  return true;
}

bool
fd_connect(int fd, const sockaddr* sa) {
  if (::connect(fd, sa, sa_length(sa)) == 0) {
    LT_LOG_FD_SOCKADDR("fd_connect succeeded");
    return true;
  }

  if (errno == EINPROGRESS) {
    LT_LOG_FD_SOCKADDR("fd_connect succeeded and in progress");
    return true;
  }

  LT_LOG_FD_SOCKADDR_ERROR("fd_connect failed");
  return false;
}

bool
fd_bind(int fd, const sockaddr* sa) {
  // Temporary workaround:
  // if (sa->sa_family == AF_INET) {
  //   sockaddr_in6 mapped;
  //   sa_inet_mapped_inet6(reinterpret_cast<const sockaddr_in*>(sa), &mapped);

  //   return ::bind(fd, reinterpret_cast<sockaddr*>(&mapped), sizeof(sockaddr_in6)) == 0;
  // }

  if (::bind(fd, sa, sa_length(sa)) == -1) {
    LT_LOG_FD_SOCKADDR_ERROR("fd_bind failed");
    return false;
  }

  LT_LOG_FD_SOCKADDR("fd_bind succeeded");
  return true;
}

bool
fd_listen(int fd, int backlog) {
  if (::listen(fd, backlog) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_listen failed", backlog);
    return false;
  }

  LT_LOG_FD_VALUE("fd_listen succeeded", backlog);
  return true;

}

}
