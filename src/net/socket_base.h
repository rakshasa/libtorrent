#ifndef LIBTORRENT_NET_SOCKET_BASE_H
#define LIBTORRENT_NET_SOCKET_BASE_H

#include <list>
#include <cinttypes>

#include "torrent/event.h"

namespace torrent {

class SocketBase : public Event {
public:
  SocketBase() = default;
  ~SocketBase() override;

  bool                read_oob(void* buffer);
  bool                write_oob(const void* buffer);

  void                receive_throttle_down_activate();
  void                receive_throttle_up_activate();

protected:
  SocketBase(const SocketBase&) = delete;
  SocketBase& operator=(const SocketBase&) = delete;

  static constexpr size_t null_buffer_size = 1 << 17;

  static char*        m_nullBuffer;
};

} // namespace torrent

#endif // LIBTORRENT_SOCKET_BASE_H
