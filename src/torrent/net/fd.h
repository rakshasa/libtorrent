#ifndef LIBTORRENT_NET_FD_H
#define LIBTORRENT_NET_FD_H

#include <string>
#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent {

enum fd_flags : int {
  fd_flag_stream        = 0x1,
  fd_flag_datagram      = 0x10,
  fd_flag_nonblock      = 0x20,
  fd_flag_reuse_address = 0x40,
  fd_flag_v4            = 0x80, // renamed
  fd_flag_v4only        = 0x80,
  fd_flag_v6only        = 0x100,
  fd_flag_all           = 0x1ff,
};

constexpr bool fd_valid_flags(fd_flags flags);

int  fd_open(fd_flags flags) LIBTORRENT_EXPORT;
int  fd_open_family(fd_flags flags, int family) LIBTORRENT_EXPORT;
void fd_open_pipe(int& fd1, int& fd2) LIBTORRENT_EXPORT;
void fd_open_socket_pair(int& fd1, int& fd2) LIBTORRENT_EXPORT;
void fd_close(int fd) LIBTORRENT_EXPORT;

int          fd_accept(int fd) LIBTORRENT_EXPORT;
fd_sap_tuple fd_sap_accept(int fd) LIBTORRENT_EXPORT;

bool fd_bind(int fd, const sockaddr* sa) LIBTORRENT_EXPORT;
bool fd_connect(int fd, const sockaddr* sa) LIBTORRENT_EXPORT;
bool fd_connect_with_family(int fd, const sockaddr* sa, int family) LIBTORRENT_EXPORT;
bool fd_listen(int fd, int backlog) LIBTORRENT_EXPORT;

bool fd_get_socket_error(int fd, int* value) LIBTORRENT_EXPORT;

bool fd_set_nonblock(int fd) LIBTORRENT_EXPORT;
bool fd_set_reuse_address(int fd, bool state) LIBTORRENT_EXPORT;
bool fd_set_priority(int fd, int family, int priority) LIBTORRENT_EXPORT;
bool fd_set_tcp_nodelay(int fd) LIBTORRENT_EXPORT;
bool fd_set_v6only(int fd, bool state) LIBTORRENT_EXPORT;

bool fd_set_send_buffer_size(int fd, uint32_t size) LIBTORRENT_EXPORT;
bool fd_set_receive_buffer_size(int fd, uint32_t size) LIBTORRENT_EXPORT;

// Defined with gnu::weak so that we can override them in tests.
[[gnu::weak]] int fd__accept(int socket, sockaddr *address, socklen_t *address_len) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__bind(int socket, const sockaddr *address, socklen_t address_len) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__close(int fildes) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__connect(int socket, const sockaddr *address, socklen_t address_len) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__fcntl_int(int fildes, int cmd, int arg) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__listen(int socket, int backlog) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__setsockopt_int(int socket, int level, int option_name, int option_value) LIBTORRENT_EXPORT;
[[gnu::weak]] int fd__socket(int domain, int type, int protocol) LIBTORRENT_EXPORT;

constexpr fd_flags
operator |(fd_flags lhs, fd_flags rhs) {
  return static_cast<fd_flags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline fd_flags&
operator |=(fd_flags& lhs, fd_flags rhs) {
  return (lhs = lhs | rhs);
}

constexpr bool
fd_valid_flags(fd_flags flags) {
  return
    ((flags & fd_flag_stream) || (flags & fd_flag_datagram)) &&
    !((flags & fd_flag_stream) && (flags & fd_flag_datagram)) &&
    !((flags & fd_flag_v4only) && (flags & fd_flag_v6only)) &&
    !(flags & ~(fd_flag_all));
}

} // namespace torrent

#endif
