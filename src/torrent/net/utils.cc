#include "config.h"

#include "torrent/net/utils.h"

#include <cerrno>
#include <cstring>
#include <netdb.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

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

c_sa_shared_ptr
lookup_address(const std::string& address_str, int family) {
  if (address_str.empty())
    return sa_make_unspec();

  // don't use rak::, use unix getaddrinfo

  addrinfo hints = {};
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* res;

  int err = ::getaddrinfo(address_str.c_str(), nullptr, &hints, &res);

  if (err != 0)
    throw input_error("Could not get address info: " + address_str + ": " + std::string(gai_strerror(err)));

  try {
    auto sa = sa_copy(res->ai_addr);
    ::freeaddrinfo(res);

    return sa;

  } catch (input_error& e) {
    ::freeaddrinfo(res);
    throw e;
  }
}

sin_unique_ptr
detect_local_sin_addr() {
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

sin6_unique_ptr
detect_local_sin6_addr() {
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
