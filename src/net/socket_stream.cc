#include "config.h"

#include "socket_stream.h"

#include <rak/error_number.h>

namespace torrent {

SocketStream::~SocketStream() = default;

uint32_t
SocketStream::read_stream_throws(void* buf, uint32_t length) {
  int r = read_stream(buf, length);

  if (r == 0)
    throw close_connection();

  if (r < 0) {
    if (rak::error_number::current().is_blocked_momentary())
      return 0;
    else if (rak::error_number::current().is_closed())
      throw close_connection();
    else if (rak::error_number::current().is_blocked_prolonged())
      throw blocked_connection();
    else
      throw connection_error(rak::error_number::current().value());
  }

  return r;
}

uint32_t
SocketStream::write_stream_throws(const void* buf, uint32_t length) {
  int r = write_stream(buf, length);

  if (r == 0)
    throw close_connection();

  if (r < 0) {
    if (rak::error_number::current().is_blocked_momentary())
      return 0;
    else if (rak::error_number::current().is_closed())
      throw close_connection();
    else if (rak::error_number::current().is_blocked_prolonged())
      throw blocked_connection();
    else
      throw connection_error(rak::error_number::current().value());
  }

  return r;
}

} // namespace torrent
