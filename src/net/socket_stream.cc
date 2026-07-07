#include "config.h"

#include "socket_stream.h"

#ifdef HAVE_SENDFILE
#include "net/sendfile_stream.h"
#endif

namespace torrent {

char* SocketStream::m_nullBuffer = new char[SocketStream::null_buffer_size];

SocketStream::~SocketStream() = default;

uint32_t
SocketStream::read_stream_throws(void* buf, uint32_t length) {
  int r = read_stream(buf, length);

  if (r == 0)
    throw close_connection();

  if (r < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return 0;
    else if (errno == ECONNRESET || errno == ECONNABORTED)
      throw close_connection();
    else if (errno == EDEADLK)
      throw blocked_connection();
    else
      throw connection_error(errno);
  }

  return r;
}

uint32_t
SocketStream::write_stream_throws(const void* buf, uint32_t length) {
  int r = write_stream(buf, length);

  if (r == 0)
    throw close_connection();

  if (r < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return 0;
    else if (errno == ECONNRESET || errno == ECONNABORTED)
      throw close_connection();
    else if (errno == EDEADLK)
      throw blocked_connection();
    else
      throw connection_error(errno);
  }

  return r;
}

uint32_t
SocketStream::write_sendfile_throws(int in_fd, uint64_t offset, uint32_t length) {
#ifdef HAVE_SENDFILE
  return sendfile_stream(file_descriptor(), in_fd, offset, length);
#else
  throw internal_error("sendfile is not supported on this platform.");
#endif
}

} // namespace torrent
