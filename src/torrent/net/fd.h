#ifndef LIBTORRENT_NET_FD_H
#define LIBTORRENT_NET_FD_H

#include <string>
#include <torrent/common.h>
#include <torrent/net/types.h>

namespace RTORRENT_EXPORT torrent {

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

constexpr bool  fd_valid_flags(fd_flags flags);

int             fd_open(fd_flags flags);
int             fd_open_family(fd_flags flags, int family);
int             fd_open_local(fd_flags flags);
void            fd_open_pipe(int& fd1, int& fd2);
void            fd_open_socket_pair(int& fd1, int& fd2);
void            fd_close(int fd);

int             fd_accept(int fd);
fd_sap_tuple    fd_sap_accept(int fd);

bool            fd_bind(int fd, const sockaddr* sa);
bool            fd_bind_with_length(int fd, const sockaddr* sa, socklen_t length);
bool            fd_connect(int fd, const sockaddr* sa);
bool            fd_connect_with_family(int fd, const sockaddr* sa, int family);
bool            fd_listen(int fd, int backlog);

bool            fd_get_nonblock(int fd, bool* value);
c_sa_unique_ptr fd_get_peer_name(int fd);
bool            fd_get_socket_error(int fd, int* value);
c_sa_unique_ptr fd_get_socket_name(int fd);
bool            fd_get_type(int fd, int* value);

bool            fd_set_dont_route(int fd, bool state);
bool            fd_set_nonblock(int fd);
bool            fd_set_reuse_address(int fd, bool state);
bool            fd_set_priority(int fd, int family, int priority);
bool            fd_set_tcp_nodelay(int fd);
bool            fd_set_v6only(int fd, bool state);

bool            fd_set_send_buffer_size(int fd, uint32_t size);
bool            fd_set_receive_buffer_size(int fd, uint32_t size);

// Defined with gnu::weak so that we can override them in tests.
[[gnu::weak]] int fd__accept(int socket, sockaddr *address, socklen_t *address_len);
[[gnu::weak]] int fd__bind(int socket, const sockaddr *address, socklen_t address_len);
[[gnu::weak]] int fd__close(int fildes);
[[gnu::weak]] int fd__connect(int socket, const sockaddr *address, socklen_t address_len);
[[gnu::weak]] int fd__fcntl_int(int fildes, int cmd, int arg);
[[gnu::weak]] int fd__listen(int socket, int backlog);
[[gnu::weak]] int fd__setsockopt_int(int socket, int level, int option_name, int option_value);
[[gnu::weak]] int fd__socket(int domain, int type, int protocol);

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
