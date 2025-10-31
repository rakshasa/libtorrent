#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <cinttypes>
#include <functional>

#include "socket_base.h"
#include "socket_fd.h"

namespace torrent {

class Listen : public SocketBase {
public:
  ~Listen() override { close(); }

  static bool         open_single(Listen* listen, const sockaddr* bind_address,
                                  uint16_t first, uint16_t last, int backlog, bool block_ipv4in6);

  static bool         open_both(Listen* listen_inet, Listen* listen_inet6,
                                const sockaddr* bind_inet_address, const sockaddr* bind_inet6_address,
                                uint16_t first, uint16_t last, int backlog, bool block_ipv4in6);

  void                close();

  bool                is_open() const { return get_fd().is_valid(); }

  uint16_t            port() const { return m_port; }

  auto&               slot_accepted() { return m_slot_accepted; }

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  void                open_done(int fd, uint16_t port, int backlog);

  uint16_t            m_port{0};

  std::function<void(int, const sockaddr*)> m_slot_accepted;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
