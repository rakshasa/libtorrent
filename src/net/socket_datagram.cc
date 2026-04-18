#include "config.h"

#include "socket_datagram.h"

#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

SocketDatagram::~SocketDatagram() = default;

int
SocketDatagram::read_datagram(void* buffer, unsigned int length) {
  if (length == 0)
    throw internal_error("Tried to receive buffer length 0");

  return ::recv(m_fileDesc, buffer, length, 0);
}

int
SocketDatagram::write_datagram(const void* buffer, unsigned int length) {
  if (length == 0)
    throw internal_error("Tried to send buffer length 0");

  return ::send(m_fileDesc, buffer, length, 0);
}

int
SocketDatagram::read_datagram_sa(void* buffer, unsigned int length, sockaddr* from_sa, socklen_t from_length) {
  if (length == 0)
    throw internal_error("Tried to receive buffer length 0");

  if (from_sa == nullptr)
    throw internal_error("Tried to receive datagram with NULL sockaddr pointer");

  return ::recvfrom(m_fileDesc, buffer, length, 0, from_sa, &from_length);
}

int
SocketDatagram::write_datagram_sa(const void* buffer, unsigned int length, sockaddr* sa) {
  if (length == 0)
    throw internal_error("Tried to send buffer length 0");

  int r;

  if (sa != nullptr)
    r = ::sendto(m_fileDesc, buffer, length, 0, sa, sa_length(sa));
  else
    r = ::send(m_fileDesc, buffer, length, 0);

  return r;
}

} // namespace torrent
