#include "config.h"

#include "net/sendfile_stream.h"

#if (defined(HAVE_SENDFILE_LINUX) && (defined(HAVE_SENDFILE_FREEBSD) || defined(HAVE_SENDFILE_DARWIN))) \
 || (defined(HAVE_SENDFILE_FREEBSD) && defined(HAVE_SENDFILE_DARWIN))
#error "Only one of HAVE_SENDFILE_LINUX, HAVE_SENDFILE_FREEBSD, HAVE_SENDFILE_DARWIN may be defined"
#endif

#if defined(HAVE_SENDFILE_LINUX)
#include <sys/sendfile.h>
#elif defined(HAVE_SENDFILE_FREEBSD) || defined(HAVE_SENDFILE_DARWIN)
#include <sys/socket.h>
#include <sys/uio.h>
#endif

#include <cerrno>
#include <limits>
#include <sys/types.h>

#include "torrent/exceptions.h"

namespace torrent {

// Linux sendfile(2): 4-arg, off_t *offset updated, returns bytes sent.
// FreeBSD sendfile(2): 7-arg, separate nbytes input and sbytes output.
// Darwin/macOS sendfile(2): 6-arg, off_t *len is value-result (in/out).
uint32_t
sendfile_stream(int socket_fd, int file_fd, uint64_t offset, uint32_t length) {
  if (length == 0)
    throw internal_error("Tried to sendfile with length 0.");

  if (offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
    throw internal_error("sendfile offset exceeds off_t maximum.");

#if defined(HAVE_SENDFILE_LINUX)
  off_t off = static_cast<off_t>(offset);
  ssize_t r = ::sendfile(socket_fd, file_fd, &off, length);

  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return 0;
    else
      throw connection_error(errno);
  }

  return static_cast<uint32_t>(r);

#elif defined(HAVE_SENDFILE_FREEBSD)
  off_t sbytes = 0;
  ssize_t r = ::sendfile(file_fd, socket_fd, static_cast<off_t>(offset), length, nullptr, &sbytes, 0);

  // EBUSY retained: documented with SF_NODISKIO; flags is 0 today but kept for future use.
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == EBUSY) {
      if (sbytes > 0)
        return static_cast<uint32_t>(sbytes);
      return 0;
    } else if (errno == EPIPE || errno == ENOTCONN)
      throw close_connection();
    else
      throw connection_error(errno);
  }

  return static_cast<uint32_t>(sbytes);

#elif defined(HAVE_SENDFILE_DARWIN)
  off_t len = length;
  ssize_t r = ::sendfile(file_fd, socket_fd, static_cast<off_t>(offset), &len, nullptr, 0);

  // EBUSY intentionally omitted: macOS sendfile(2) does not document it.
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      if (len > 0)
        return static_cast<uint32_t>(len);
      return 0;
    } else if (errno == EPIPE || errno == ENOTCONN)
      throw close_connection();
    else
      throw connection_error(errno);
  }

  return static_cast<uint32_t>(len);

#else
  throw internal_error("sendfile is not supported on this platform.");
#endif
}

} // namespace torrent
