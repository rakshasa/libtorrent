#ifndef LIBTORRENT_NET_SOCKET_FD_H
#define LIBTORRENT_NET_SOCKET_FD_H

#include <cstdint>

#include <unistd.h>

struct sockaddr;

namespace rak {
  class socket_address;
}

namespace torrent {

class SocketFd {
public:
  using priority_type = uint8_t;

  SocketFd() = default;
  explicit SocketFd(int fd) : m_fd(fd) {}
  SocketFd(int fd, bool ipv6_socket) : m_fd(fd), m_ipv6_socket(ipv6_socket) {}

  bool                is_valid() const                        { return m_fd >= 0; }

  int                 get_fd() const                          { return m_fd; }
  void                set_fd(int fd)                          { m_fd = fd; }

  bool                set_nonblock();
  bool                set_reuse_address(bool state);
  bool                set_ipv6_v6only(bool state);

  bool                set_priority(priority_type p);

  bool                set_send_buffer_size(uint32_t s);
  bool                set_receive_buffer_size(uint32_t s);

  int                 get_error() const;

  bool                open_stream();
  bool                open_datagram();
  bool                open_local();

  static bool         open_socket_pair(int& fd1, int& fd2);

  void                close();
  void                clear() { m_fd = -1; }

  bool                bind(const rak::socket_address& sa);
  bool                bind(const rak::socket_address& sa, unsigned int length);
  bool                bind_sa(const sockaddr* sa);

  bool                connect(const rak::socket_address& sa);
  bool                connect_sa(const sockaddr* sa);

  bool                getsockname(rak::socket_address* sa);

  bool                listen(int size);
  SocketFd            accept(rak::socket_address* sa);

//   unsigned int        get_read_queue_size() const;
//   unsigned int        get_write_queue_size() const;

private:
  inline void         check_valid() const;

  int                 m_fd{-1};
  bool                m_ipv6_socket{};
};

}

#endif
