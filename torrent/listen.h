#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <set>
#include "socket_base.h"

namespace torrent {

class Listen : public SocketBase {
public:
  static void open(int first, int last);
  static void close();

  static unsigned short port() { return m_listen ? m_listen->m_port : 0; }

  virtual void read();
  virtual void write();
  virtual void except();
  virtual int fd();

private:
  static Listen* m_listen;

  unsigned short m_port;
  int m_fd;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
