#include <torrent/net/utils.h>

#include <cerrno>
#include <cstring>
#include <torrent/net/fd.h>
#include <torrent/net/socket_address.h>
#include <torrent/utils/log.h>

#define LT_LOG_ERROR(log_fmt)                                                                                \
  do {                                                                                                       \
    lt_log_print(LOG_CONNECTION_FD, "fd: " log_fmt " (errno:%i message:'%s')", errno, std::strerror(errno)); \
  } while (false)
#define LT_LOG_FD(log_fmt)                                   \
  do {                                                       \
    lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt, fd); \
  } while (false)
#define LT_LOG_FD_ERROR(log_fmt)                                                                                     \
  do {                                                                                                               \
    lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (errno:%i message:'%s')", fd, errno, std::strerror(errno)); \
  } while (false)
#define LT_LOG_FD_SIN(log_fmt)                                                                                 \
  do {                                                                                                         \
    lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s)", fd, sin_pretty_str(sa.get()).c_str()); \
  } while (false)
#define LT_LOG_FD_SIN6(log_fmt)                                                                                 \
  do {                                                                                                          \
    lt_log_print(LOG_CONNECTION_FD, "fd->%i: " log_fmt " (address:%s)", fd, sin6_pretty_str(sa.get()).c_str()); \
  } while (false)

namespace torrent {

auto detect_local_sin_addr() -> sin_unique_ptr {
  int fd = fd_open(fd_flag_v4only | fd_flag_datagram);
  if (fd == -1) {
    LT_LOG_ERROR("detect_local_sin_addr: open failed");
    return sin_unique_ptr();
  }

  auto connectAddress = sin_make();
  connectAddress->sin_addr.s_addr = htonl(0x04000001);
  connectAddress->sin_port = 80;

  if (!fd_connect(fd, reinterpret_cast<sockaddr*>(connectAddress.get())) && errno != EINPROGRESS) {
    fd_close(fd);
    LT_LOG_FD_ERROR("detect_local_sin_addr: connect failed");
    return sin_unique_ptr();
  }

  // TODO: Make sa function.
  socklen_t socklen = sizeof(sockaddr_in);

  auto sa = sin_make();

  if (::getsockname(fd, reinterpret_cast<sockaddr*>(sa.get()), &socklen) != 0) {
    fd_close(fd);
    LT_LOG_FD_ERROR("detect_local_sin_addr: getsockname failed");
    return sin_unique_ptr();
  }
  if (socklen != sizeof(sockaddr_in)) {
    fd_close(fd);
    LT_LOG_FD("detect_local_sin_addr: getsockname failed, invalid socklen");
    return sin_unique_ptr();
  }

  fd_close(fd);
  LT_LOG_FD_SIN("detect_local_sin_addr: success");

  return sa;
}

auto detect_local_sin6_addr() -> sin6_unique_ptr {
  int fd = fd_open(fd_flag_v6only | fd_flag_datagram);
  if (fd == -1) {
    LT_LOG_ERROR("detect_local_sin6_addr: open failed");
    return sin6_unique_ptr();
  }

  auto connectAddress = sin6_make();
  connectAddress->sin6_addr.s6_addr[0] = 0x20;
  connectAddress->sin6_addr.s6_addr[1] = 0x01;
  connectAddress->sin6_addr.s6_addr[15] = 0x01;
  connectAddress->sin6_port = 80;

  if (!fd_connect(fd, reinterpret_cast<sockaddr*>(connectAddress.get())) && errno != EINPROGRESS) {
    fd_close(fd);
    LT_LOG_FD_ERROR("detect_local_sin6_addr: connect failed");
    return sin6_unique_ptr();
  }

  // TODO: Make sa function.
  socklen_t socklen = sizeof(sockaddr_in6);

  auto sa = sin6_make();

  if (::getsockname(fd, reinterpret_cast<sockaddr*>(sa.get()), &socklen) != 0) {
    fd_close(fd);
    LT_LOG_FD_ERROR("detect_local_sin6_addr: getsockname failed");
    return sin6_unique_ptr();
  }
  if (socklen != sizeof(sockaddr_in6)) {
    fd_close(fd);
    LT_LOG_FD("detect_local_sin6_addr: getsockname failed, invalid socklen");
    return sin6_unique_ptr();
  }

  fd_close(fd);
  LT_LOG_FD_SIN6("detect_local_sin6_addr: success");

  return sa;
}

} // namespace torrent
