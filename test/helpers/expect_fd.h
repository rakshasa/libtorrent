#ifndef LIBTORRENT_HELPER_EXPECT_FD_H
#define LIBTORRENT_HELPER_EXPECT_FD_H

#include "helpers/mock_function.h"

#include <fcntl.h>
#include <torrent/event.h>
#include <torrent/net/fd.h>
#include <torrent/net/socket_address.h>

typedef std::vector<torrent::sa_unique_ptr> sap_cache_type;

inline const sockaddr*
sap_cache_copy_addr_c_ptr(sap_cache_type& sap_cache, const torrent::c_sa_unique_ptr& sap, uint16_t port = 0) {
  sap_cache.push_back(torrent::sap_copy_addr(sap, port));
  return sap_cache.back().get();
}

inline void
expect_event_open_re(int idx) {
  mock_expect(&torrent::poll_event_open, mock_compare_map<torrent::Event>::begin_pointer() + idx);
  mock_expect(&torrent::poll_event_insert_read, mock_compare_map<torrent::Event>::begin_pointer() + idx);
  mock_expect(&torrent::poll_event_insert_error, mock_compare_map<torrent::Event>::begin_pointer() + idx);
}

inline void
expect_event_closed_fd(int idx, int fd) {
  mock_expect(&torrent::fd__close, 0, fd);
  mock_expect(&torrent::poll_event_closed, mock_compare_map<torrent::Event>::begin_pointer() + idx);
}

inline void
expect_fd_inet_tcp(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
}

inline void
expect_fd_inet6_tcp(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
}

inline void
expect_fd_inet_tcp_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
expect_fd_inet6_tcp_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
expect_fd_inet_tcp_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
expect_fd_inet6_tcp_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
expect_fd_inet6_tcp_v6only_nonblock(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
}

inline void
expect_fd_inet6_tcp_v6only_nonblock_reuseaddr(int fd) {
  mock_expect(&torrent::fd__socket, fd, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
  mock_expect(&torrent::fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&torrent::fd__setsockopt_int, 0, fd, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
}

inline void
expect_fd_bind_connect(int fd, const torrent::c_sa_unique_ptr& bind_sap, const torrent::c_sa_unique_ptr& connect_sap) {
  mock_expect(&torrent::fd__bind, 0, fd, bind_sap.get(), (socklen_t)torrent::sap_length(bind_sap));
  mock_expect(&torrent::fd__connect, 0, fd, connect_sap.get(), (socklen_t)torrent::sap_length(connect_sap));
}

inline void
expect_fd_bind_fail_range(int fd, sap_cache_type& sap_cache, const torrent::c_sa_unique_ptr& sap, uint16_t first_port, uint16_t last_port) {
  do {
    mock_expect(&torrent::fd__bind, -1, fd, sap_cache_copy_addr_c_ptr(sap_cache, sap, first_port), (socklen_t)torrent::sap_length(sap));
  } while (first_port++ != last_port);
}

inline void
expect_fd_bind_listen(int fd, const torrent::c_sa_unique_ptr& sap) {
  mock_expect(&torrent::fd__bind, 0, fd, sap.get(), (socklen_t)torrent::sap_length(sap));
  mock_expect(&torrent::fd__listen, 0, fd, SOMAXCONN);
}

inline void
expect_fd_connect(int fd, const torrent::c_sa_unique_ptr& sap) {
  mock_expect(&torrent::fd__connect, 0, fd, sap.get(), (socklen_t)torrent::sap_length(sap));
}

#endif
