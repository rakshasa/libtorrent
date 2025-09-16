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

  bool                open(uint16_t first, uint16_t last, int backlog, const sockaddr* bind_address);
  void                close();

  bool                is_open() const { return get_fd().is_valid(); }

  uint16_t            port() const { return m_port; }

  auto&               slot_accepted() { return m_slot_accepted; }

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  uint64_t            m_port{0};

  std::function<void(int, const sockaddr*)> m_slot_accepted;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
