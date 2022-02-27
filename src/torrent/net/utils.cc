#import <torrent/net/utils.h>

#import <cerrno>
#import <cstring>
#import <torrent/net/fd.h>
#import <torrent/net/socket_address.h>
#import <torrent/utils/log.h>

#define LT_LOG_ERROR(log_fmt)                                           \
  lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " (errno:%i message:'%s')", \
               errno, std::strerror(errno));
#define LT_LOG_FD(log_fmt)                                              \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt, fd);
#define LT_LOG_FD_ERROR(log_fmt)                                        \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (errno:%i message:'%s')", \
               fd, errno, std::strerror(errno));
#define LT_LOG_FD_SIN(log_fmt)                                        \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s)", \
               fd, sin_pretty_str(sa.get()).c_str());
#define LT_LOG_FD_SIN6(log_fmt)                                       \
  lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s)", \
               fd, sin6_pretty_str(sa.get()).c_str());

namespace torrent {

auto detect_local_sin_addr() -> sin_unique_ptr {
  int fd = fd_open(fd_flag_v4only | fd_flag_datagram);
  if (fd == -1) {
    LT_LOG_ERROR("detect_local_sin_addr: open failed");
    return sin_unique_ptr();
  }

  // TODO: Check if unique_ptr works.
  std::shared_ptr<void> _fd(nullptr, [fd](...){ fd_close(fd); });

  auto connectAddress = sin_make();
  connectAddress.get()->sin_addr.s_addr = htonl(0x04000001);
  connectAddress.get()->sin_port = 80;

  if (!fd_connect(fd, reinterpret_cast<sockaddr*>(connectAddress.get())) && errno != EINPROGRESS) {
    LT_LOG_FD_ERROR("detect_local_sin_addr: connect failed");
    return sin_unique_ptr();
  }

  // TODO: Make sa function.
  socklen_t socklen = sizeof(sockaddr_in);

  auto sa = sin_make();

  if (::getsockname(fd, reinterpret_cast<sockaddr*>(sa.get()), &socklen) != 0) {
    LT_LOG_FD_ERROR("detect_local_sin_addr: getsockname failed");
    return sin_unique_ptr();
  }
  if (socklen != sizeof(sockaddr_in)) {
    LT_LOG_FD("detect_local_sin_addr: getsockname failed, invalid socklen");
    return sin_unique_ptr();
  }

  LT_LOG_FD_SIN("detect_local_sin_addr: success");

  return sa;
}

auto detect_local_sin6_addr() -> sin6_unique_ptr {
  int fd = fd_open(fd_flag_v6only | fd_flag_datagram);
  if (fd == -1) {
    LT_LOG_ERROR("detect_local_sin6_addr: open failed");
    return sin6_unique_ptr();
  }

  // TODO: Check if unique_ptr works.
  std::shared_ptr<void> _fd(nullptr, [fd](...){ fd_close(fd); });

  auto connectAddress = sin6_make();
  connectAddress.get()->sin6_addr.s6_addr[0] = 0x20;
  connectAddress.get()->sin6_addr.s6_addr[1] = 0x01;
  connectAddress.get()->sin6_addr.s6_addr[15] = 0x01;
  connectAddress.get()->sin6_port = 80;

  if (!fd_connect(fd, reinterpret_cast<sockaddr*>(connectAddress.get())) && errno != EINPROGRESS) {
    LT_LOG_FD_ERROR("detect_local_sin6_addr: connect failed");
    return sin6_unique_ptr();
  }

  // TODO: Make sa function.
  socklen_t socklen = sizeof(sockaddr_in6);

  auto sa = sin6_make();

  if (::getsockname(fd, reinterpret_cast<sockaddr*>(sa.get()), &socklen) != 0) {
    LT_LOG_FD_ERROR("detect_local_sin6_addr: getsockname failed");
    return sin6_unique_ptr();
  }
  if (socklen != sizeof(sockaddr_in6)) {
    LT_LOG_FD("detect_local_sin6_addr: getsockname failed, invalid socklen");
    return sin6_unique_ptr();
  }

  LT_LOG_FD_SIN6("detect_local_sin6_addr: success");

  return sa;
}

}
