#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <sigc/signal.h>
#include "socket_base.h"

namespace torrent {

class Listen : public SocketBase {
public:
  typedef sigc::signal3<void, int, std::string, uint16_t> SignalIncoming;

  Listen() : m_fd(-1), m_port(0) {}
  ~Listen() { close(); }

  bool            open(uint16_t first, uint16_t last);
  void            close();

  bool            is_open()         { return m_fd >= 0; }

  uint16_t        get_port()        { return m_port; }

  SignalIncoming& signal_incoming() { return m_incoming; }

  virtual void read();
  virtual void write();
  virtual void except();
  virtual int  fd();

private:
  int m_fd;
  unsigned short m_port;

  SignalIncoming m_incoming;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
