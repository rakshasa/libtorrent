#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <inttypes.h>
#include <sigc++/signal.h>

#include "socket_base.h"

namespace torrent {

class Listen : public SocketBase {
public:
  typedef sigc::slot3<void, int, std::string, uint16_t> SlotIncoming;

  Listen() : SocketBase(-1), m_port(0) {}
  ~Listen() { close(); }

  bool            open(uint16_t first, uint16_t last);
  void            close();

  bool            is_open()                            { return m_fd >= 0; }

  uint16_t        get_port()                           { return m_port; }

  // int         file descriptor
  // std::string address
  // uint16_t    port
  void            slot_incoming(const SlotIncoming& s) { m_slotIncoming = s; }

  virtual void    read();
  virtual void    write();
  virtual void    except();

private:
  Listen(const Listen&);
  void operator = (const Listen&);

  uint64_t        m_port;

  SlotIncoming    m_slotIncoming;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
