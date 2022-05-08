#include "config.h"

#include "fd.h"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

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

int fd__accept(int socket, sockaddr *address, socklen_t *address_len) { return ::accept(socket, address, address_len); }
int fd__bind(int socket, const sockaddr *address, socklen_t address_len) { return ::bind(socket, address, address_len); }
int fd__close(int fildes) { return ::close(fildes); }
int fd__connect(int socket, const sockaddr *address, socklen_t address_len) { return ::connect(socket, address, address_len); }
int fd__fcntl_int(int fildes, int cmd, int arg) { return ::fcntl(fildes, cmd, arg); }
int fd__listen(int socket, int backlog) { return ::listen(socket, backlog); }
int fd__setsockopt_int(int socket, int level, int option_name, int option_value) { return ::setsockopt(socket, level, option_name, &option_value, sizeof(int)); }
int fd__socket(int domain, int type, int protocol) { return ::socket(domain, type, protocol); }

int
fd_open(fd_flags flags) {
  int domain;
  int protocol;

  if (!fd_valid_flags(flags))
    throw internal_error("torrent::fd_open failed: invalid fd_flags");

  if ((flags & fd_flag_stream)) {
    domain = SOCK_STREAM;
    protocol = IPPROTO_TCP;
  } else if ((flags & fd_flag_datagram)) {
    domain = SOCK_DGRAM;
    protocol = IPPROTO_UDP;
  } else {
    LT_LOG_FLAG("fd_open missing socket type");
    errno = EINVAL;
    return -1;
  }

  int fd = -1;

  if (fd == -1 && !(flags & fd_flag_v4only)) {
    LT_LOG_FLAG("fd_open opening ipv6 socket");
    fd = fd__socket(PF_INET6, domain, protocol);
  }

  if (fd == -1 && !(flags & fd_flag_v6only)) {
    LT_LOG_FLAG("fd_open opening ipv4 socket");
    fd = fd__socket(PF_INET, domain, protocol);
  }

  if (fd == -1) {
    LT_LOG_FLAG_ERROR("fd_open failed to open socket");
    return -1;
  }

  if ((flags & fd_flag_v6only) && !fd_set_v6only(fd, true)) {
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
    throw internal_error("torrent::fd_close: tried to close stdin/out/err");

  if (fd__close(fd) == -1)
    throw internal_error("torrent::fd_close: " + std::string(strerror(errno)));

  LT_LOG_FD("fd_close succeeded");
}

fd_sap_tuple
fd_accept(int fd) {
  sa_unique_ptr sap = sa_make_inet6();
  socklen_t socklen = sap_length(sap);

  int accept_fd = fd__accept(fd, sap.get(), &socklen);

  if (accept_fd == -1) {
    LT_LOG_FD_ERROR("fd_accept failed");
    return fd_sap_tuple{-1, nullptr};
  }

  return fd_sap_tuple{accept_fd, std::move(sap)};
}

bool
fd_bind(int fd, const sockaddr* sa) {
  if (fd__bind(fd, sa, sa_length(sa)) == -1) {
    LT_LOG_FD_SOCKADDR_ERROR("fd_bind failed");
    return false;
  }

  LT_LOG_FD_SOCKADDR("fd_bind succeeded");
  return true;
}

bool
fd_connect(int fd, const sockaddr* sa) {
  if (fd__connect(fd, sa, sa_length(sa)) == 0) {
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
fd_listen(int fd, int backlog) {
  if (fd__listen(fd, backlog) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_listen failed", backlog);
    return false;
  }

  LT_LOG_FD_VALUE("fd_listen succeeded", backlog);
  return true;
}

bool
fd_set_nonblock(int fd) {
  if (fd__fcntl_int(fd, F_SETFL, O_NONBLOCK) == -1) {
    LT_LOG_FD_ERROR("fd_set_nonblock failed");
    return false;
  }

  LT_LOG_FD("fd_set_nonblock succeeded");
  return true;
}

bool
fd_set_reuse_address(int fd, bool state) {
  if (fd__setsockopt_int(fd, SOL_SOCKET, SO_REUSEADDR, state) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_reuse_address failed", state);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_reuse_address succeeded", state);
  return true;
}

bool
fd_set_v6only(int fd, bool state) {
  if (fd__setsockopt_int(fd, IPPROTO_IPV6, IPV6_V6ONLY, state) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_v6only failed", state);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_v6only succeeded", state);
  return true;
}

}
