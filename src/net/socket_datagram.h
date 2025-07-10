#ifndef LIBTORRENT_NET_SOCKET_DGRAM_H
#define LIBTORRENT_NET_SOCKET_DGRAM_H

#include "socket_base.h"

namespace torrent {

class SocketDatagram : public SocketBase {
public:
  ~SocketDatagram() override;

  // TODO: Make two seperate functions depending on whetever sa is
  // used.
  int                 read_datagram(void* buffer, unsigned int length, rak::socket_address* sa = NULL);
  int                 write_datagram(const void* buffer, unsigned int length, rak::socket_address* sa = NULL);

  int                 write_datagram_sa(const void* buffer, unsigned int length, sockaddr* sa);
};

} // namespace torrent

#endif
