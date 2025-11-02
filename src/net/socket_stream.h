#ifndef LIBTORRENT_NET_SOCKET_STREAM_H
#define LIBTORRENT_NET_SOCKET_STREAM_H

#include <sys/types.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "socket_base.h"

namespace torrent {

class SocketStream : public SocketBase {
public:
  ~SocketStream() override;

  int                 read_stream(void* buf, uint32_t length);
  int                 write_stream(const void* buf, uint32_t length);

  // Returns the number of bytes read, or zero if the socket is
  // blocking. On errors or closed sockets it will throw an
  // appropriate exception.
  uint32_t            read_stream_throws(void* buf, uint32_t length);
  uint32_t            write_stream_throws(const void* buf, uint32_t length);

  // Handles all the error catching etc. Returns true if the buffer is
  // finished reading/writing.
  bool                read_buffer(void* buf, uint32_t length, uint32_t& pos);
  bool                write_buffer(const void* buf, uint32_t length, uint32_t& pos);

  uint32_t            ignore_stream_throws(uint32_t length) { return read_stream_throws(m_nullBuffer, length); }
};

inline bool
SocketStream::read_buffer(void* buf, uint32_t length, uint32_t& pos) {
  pos += read_stream_throws(buf, length - pos);

  return pos == length;
}

inline bool
SocketStream::write_buffer(const void* buf, uint32_t length, uint32_t& pos) {
  pos += write_stream_throws(buf, length - pos);

  return pos == length;
}

inline int
SocketStream::read_stream(void* buf, uint32_t length) {
  if (length == 0)
    throw internal_error("Tried to read to buffer length 0.");

  return ::recv(m_fileDesc, buf, length, 0);
}

inline int
SocketStream::write_stream(const void* buf, uint32_t length) {
  if (length == 0)
    throw internal_error("Tried to write to buffer length 0.");

  return ::send(m_fileDesc, buf, length, 0);
}

} // namespace torrent

#endif
