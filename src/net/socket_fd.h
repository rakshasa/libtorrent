#ifndef LIBTORRENT_NET_SOCKET_FD_H
#define LIBTORRENT_NET_SOCKET_FD_H

#include <cstdint>

struct sockaddr;

namespace torrent {

class SocketFd {
public:
  using priority_type = uint8_t;

  SocketFd() = default;
  explicit SocketFd(int fd) : m_fd(fd) {}
  SocketFd(int fd, bool ipv6_socket) : m_fd(fd), m_ipv6_socket(ipv6_socket) {}

  bool                is_valid() const                        { return m_fd >= 0; }
  bool                is_ipv6_socket() const                  { return m_ipv6_socket; }

  int                 get_fd() const                          { return m_fd; }
  void                set_fd(int fd)                          { m_fd = fd; }

  bool                set_nonblock();
  bool                set_reuse_address(bool state);
  bool                set_ipv6_v6only(bool state);

  bool                open_stream();
  bool                open_datagram();
  bool                open_local();

  static bool         open_socket_pair(int& fd1, int& fd2);

  void                close() const;
  void                clear() { m_fd = -1; }

private:
  inline void         check_valid() const;

  int                 m_fd{-1};
  bool                m_ipv6_socket{};
};

} // namespace torrent

#endif
