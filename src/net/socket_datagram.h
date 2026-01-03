#ifndef LIBTORRENT_NET_SOCKET_DGRAM_H
#define LIBTORRENT_NET_SOCKET_DGRAM_H

#include <sys/socket.h>

#include "torrent/event.h"

namespace torrent {

class SocketDatagram : public Event {
public:
  ~SocketDatagram() override;

  int                 read_datagram(void* buffer, unsigned int length);
  int                 write_datagram(const void* buffer, unsigned int length);

  int                 read_datagram_sa(void* buffer, unsigned int length, sockaddr* from_sa, socklen_t from_length);
  int                 write_datagram_sa(const void* buffer, unsigned int length, sockaddr* sa);
};

} // namespace torrent

#endif
