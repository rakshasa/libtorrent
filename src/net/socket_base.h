#ifndef LIBTORRENT_NET_SOCKET_BASE_H
#define LIBTORRENT_NET_SOCKET_BASE_H

#include <list>
#include <cinttypes>

#include "torrent/event.h"
#include "socket_fd.h"

namespace torrent {

class SocketBase : public Event {
public:
  SocketBase() { set_fd(SocketFd()); }
  ~SocketBase() override;
  SocketBase(const SocketBase&) = delete;
  SocketBase& operator=(const SocketBase&) = delete;

  // Ugly hack... But the alternative is to include SocketFd as part
  // of the library API or make SocketFd::m_fd into an non-modifiable
  // value.
  SocketFd&           get_fd()            { return *reinterpret_cast<SocketFd*>(&m_fileDesc); }
  const SocketFd&     get_fd() const      { return *reinterpret_cast<const SocketFd*>(&m_fileDesc); }
  void                set_fd(SocketFd fd) { m_fileDesc = fd.get_fd(); }

  bool                read_oob(void* buffer);
  bool                write_oob(const void* buffer);

  void                receive_throttle_down_activate();
  void                receive_throttle_up_activate();

protected:
  static constexpr size_t null_buffer_size = 1 << 17;

  static char*        m_nullBuffer;
};

} // namespace torrent

#endif // LIBTORRENT_SOCKET_BASE_H
