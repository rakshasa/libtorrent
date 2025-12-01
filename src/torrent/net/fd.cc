#include "config.h"

#include "fd.h"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                    \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt, __VA_ARGS__);
#define LT_LOG_FLAG(log_fmt)                                            \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " : flags:0x%x", flags);
#define LT_LOG_FLAG_ERROR(log_fmt)                                      \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " : flags:0x%x errno:%i message:'%s'", \
               flags, errno, std::strerror(errno));
#define LT_LOG_FD(log_fmt)                                      \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt, fd);
#define LT_LOG_FD_ERROR(log_fmt)                                        \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : errno:%i message:'%s'", \
               fd, errno, std::strerror(errno));
#define LT_LOG_FD_SOCKADDR(log_fmt)                                   \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : address:%s", \
               fd, sa_pretty_str(sa).c_str());
#define LT_LOG_FD_SOCKADDR_ERROR(log_fmt)                               \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : address:%s errno:%i message:'%s'", \
               fd, sa_pretty_str(sa).c_str(), errno, std::strerror(errno));
#define LT_LOG_FD_SAP(log_fmt)                                        \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : address:%s", \
               fd, sap_pretty_str(sap).c_str());
#define LT_LOG_FD_FLAG(log_fmt)                                         \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : flags:0x%x", fd, flags);
#define LT_LOG_FD_FLAG_ERROR(log_fmt)                                   \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : flags:0x%x errno:%i message:'%s'", \
               fd, flags, errno, std::strerror(errno));
#define LT_LOG_FD_VALUE(log_fmt, value)                                 \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : value:%i", fd, (int)value);
#define LT_LOG_FD_VALUE_ERROR(log_fmt, value)                           \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " : value:%i errno:%i message:'%s'", \
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

int
fd_open_family(fd_flags flags, int family) {
  if (family != AF_INET && family != AF_INET6) {
    LT_LOG_FLAG("fd_open_family invalid family");
    errno = EINVAL;
    return -1;
  }

  if (family == AF_INET)
    flags |= fd_flag_v4;

  return fd_open(flags);
}

void
fd_open_pipe(int& fd1, int& fd2) {
  int result[2];

  if (pipe(result) == -1)
    throw internal_error("torrent::fd_open_pipe failed: " + std::string(strerror(errno)));

  fd1 = result[0];
  fd2 = result[1];

  LT_LOG("fd_open_pipe succeeded : fd1:%i fd2:%i", fd1, fd2);
}

void
fd_open_socket_pair(int& fd1, int& fd2) {
  int result[2];

  if (socketpair(AF_LOCAL, SOCK_STREAM, 0, result) == -1)
    throw internal_error("torrent::fd_open_socket_pair failed: " + std::string(strerror(errno)));

  fd1 = result[0];
  fd2 = result[1];

  LT_LOG("fd_open_socket_pair succeeded : fd1:%i fd2:%i", fd1, fd2);
}

void
fd_close(int fd) {
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
    throw internal_error("torrent::fd_close: tried to close stdin/out/err");

  if (fd__close(fd) == -1)
    throw internal_error("torrent::fd_close: " + std::string(strerror(errno)));

  LT_LOG_FD("fd_close succeeded");
}

int
fd_accept(int fd) {
  int connection_fd = fd__accept(fd, nullptr, nullptr);

  if (connection_fd == -1) {
    LT_LOG_FD_ERROR("fd_accept() failed");
    return -1;
  }

  LT_LOG_FD("fd_accept() succeeded");
  return connection_fd;
}

fd_sap_tuple
fd_sap_accept(int fd) {
  sa_inet_union sau{};
  socklen_t sau_length = sizeof(sockaddr_in6);

  int connection_fd = fd__accept(fd, &sau.sa, &sau_length);

  if (connection_fd == -1) {
    LT_LOG_FD_ERROR("fd_sap_accept() failed");
    return fd_sap_tuple{-1, nullptr};
  }

  auto sap = sa_copy(&sau.sa);

  LT_LOG_FD_SAP("fd_sap_accept() succeeded");
  return fd_sap_tuple{connection_fd, std::move(sap)};
}

bool
fd_bind(int fd, const sockaddr* sa) {
  if (fd__bind(fd, sa, sa_length(sa)) == -1) {
    LT_LOG_FD_SOCKADDR_ERROR("fd_bind() failed");
    return false;
  }

  LT_LOG_FD_SOCKADDR("fd_bind() succeeded");
  return true;
}

bool
fd_connect(int fd, const sockaddr* sa) {
  if (fd__connect(fd, sa, sa_length(sa)) == 0) {
    LT_LOG_FD_SOCKADDR("fd_connect() succeeded");
    return true;
  }

  if (errno == EINPROGRESS) {
    LT_LOG_FD_SOCKADDR("fd_connect() succeeded and in progress");
    return true;
  }

  LT_LOG_FD_SOCKADDR_ERROR("fd_connect() failed");
  return false;
}

bool
fd_connect_with_family(int fd, const sockaddr* sa, int family) {
  switch (sa->sa_family) {
  case AF_UNSPEC:
    errno = EINVAL;
    LT_LOG_FD("fd_connect_with_family() cannot connect unspecified address");
    return false;

  case AF_INET:
    if (family == AF_INET6) {
      LT_LOG_FD_SOCKADDR("fd_connect_with_family() connecting ipv4 using ipv6");
      return fd_connect(fd, sa_to_v4mapped(sa).get());
    }

    LT_LOG_FD_SOCKADDR("fd_connect_with_family() connecting ipv4");
    return fd_connect(fd, sa);

  case AF_INET6:
    if (family == AF_INET) {
      if (sa_is_v4mapped(sa)) {
        LT_LOG_FD_SOCKADDR("fd_connect_with_family() connecting ipv4in6 as ipv4");
        return fd_connect(fd, sa_from_v4mapped(sa).get());
      }

      errno = EINVAL;
      LT_LOG_FD("fd_connect_with_family() cannot connect ipv6 address with ipv4 bind");
      return false;
    }

    LT_LOG_FD_SOCKADDR("fd_connect_with_family() connecting ipv6");
    return fd_connect(fd, sa);

  default:
    errno = EINVAL;
    LT_LOG_FD_VALUE("fd_connect_with_family() invalid sa_family", sa->sa_family);
    return false;
  }
}

bool
fd_listen(int fd, int backlog) {
  if (fd__listen(fd, backlog) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_listen() failed", backlog);
    return false;
  }

  LT_LOG_FD_VALUE("fd_listen() succeeded", backlog);
  return true;
}

bool
fd_get_socket_error(int fd, int* value) {
  socklen_t length = sizeof(int);

  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, value, &length) == -1) {
    LT_LOG_FD_ERROR("fd_get_socket_error() failed");
    return false;
  }

  LT_LOG_FD_VALUE("fd_get_socket_error() succeeded", *value);
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
fd_set_priority(int fd, int family, int priority) {
  int level;
  int opt;

  switch (family) {
    case AF_INET:
      level = IPPROTO_IP;
      opt = IP_TOS;
      break;
    case AF_INET6:
      level = IPPROTO_IPV6;
      opt = IPV6_TCLASS;
      break;
    default:
      errno = EINVAL;
      LT_LOG_FD_VALUE("fd_set_priority invalid family", family);
      return false;
  }

  if (fd__setsockopt_int(fd, level, opt, priority) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_priority failed", priority);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_priority succeeded", priority);
  return true;
}

bool
fd_set_tcp_nodelay(int fd) {
  if (fd__setsockopt_int(fd, IPPROTO_TCP, TCP_NODELAY, true) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_tcp_nodelay failed", true);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_tcp_nodelay succeeded", true);
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

bool
fd_set_send_buffer_size(int fd, uint32_t size) {
  int opt = size;

  if (fd__setsockopt_int(fd, SOL_SOCKET, SO_SNDBUF, opt) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_send_buffer_size failed", opt);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_send_buffer_size succeeded", opt);
  return true;
}

bool
fd_set_receive_buffer_size(int fd, uint32_t size) {
  int opt = size;

  if (fd__setsockopt_int(fd, SOL_SOCKET, SO_RCVBUF, opt) == -1) {
    LT_LOG_FD_VALUE_ERROR("fd_set_receive_buffer_size failed", opt);
    return false;
  }

  LT_LOG_FD_VALUE("fd_set_receive_buffer_size succeeded", opt);
  return true;
}

} // namespace torrent
