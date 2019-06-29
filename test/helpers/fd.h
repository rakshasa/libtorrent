#ifndef LIBTORRENT_HELPER_FD_H
#define LIBTORRENT_HELPER_FD_H

#include <fcntl.h>

#include "helpers/mock_function.h"
#include "torrent/net/fd.h"

inline void
fd_expect_inet_tcp(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
}

inline void
fd_expect_inet6_tcp(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
}

inline void
fd_expect_inet_tcp_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
fd_expect_inet6_tcp_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
fd_expect_inet_tcp_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
fd_expect_inet6_tcp_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
fd_expect_inet6_tcp_v6only_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
fd_expect_inet6_tcp_v6only_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
fd_expect_bind_listen(int fd, const torrent::c_sa_unique_ptr& sap) {
  mock_expect(&torrent::fd__bind, 0, fd, sap.get(), (socklen_t)torrent::sap_length(sap));
  mock_expect(&torrent::fd__listen, 0, fd, SOMAXCONN);
}

inline void
fd_expect_bind_connect(int fd, const torrent::c_sa_unique_ptr& bind_sap, const torrent::c_sa_unique_ptr& connect_sap) {
  mock_expect(&torrent::fd__bind, 0, 1000, bind_sap.get(), (socklen_t)torrent::sap_length(bind_sap));
  mock_expect(&torrent::fd__connect, 0, 1000, connect_sap.get(), (socklen_t)torrent::sap_length(connect_sap));
}

inline void
fd_expect_connect(int fd, const torrent::c_sa_unique_ptr& sap) {
  mock_expect(&torrent::fd__connect, 0, 1000, sap.get(), (socklen_t)torrent::sap_length(sap));
}

#endif
