#include "config.h"

#include "net/socket_base.h"

#include <cassert>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "torrent/poll.h"
#include "torrent/utils/thread.h"

namespace torrent {

char* SocketBase::m_nullBuffer = new char[SocketBase::null_buffer_size];

SocketBase::~SocketBase() {
  assert(!get_fd().is_valid() && "SocketBase::~SocketBase() called but m_fd is still valid");
}

bool
SocketBase::read_oob(void* buffer) {
  int r = ::recv(m_fileDesc, buffer, 1, MSG_OOB);

//   if (r < 0)
//     m_errno = errno;

  return r == 1;
}

bool
SocketBase::write_oob(const void* buffer) {
  int r = ::send(m_fileDesc, buffer, 1, MSG_OOB);

//   if (r < 0)
//     m_errno = errno;

  return r == 1;
}

void
SocketBase::receive_throttle_down_activate() {
  thread_self->poll()->insert_read(this);
}

void
SocketBase::receive_throttle_up_activate() {
  thread_self->poll()->insert_write(this);
}

} // namespace torrent
