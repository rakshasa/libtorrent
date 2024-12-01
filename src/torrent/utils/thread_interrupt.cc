#include "config.h"

#include "thread_interrupt.h"

#include <sys/socket.h>

#include "net/socket_fd.h"
#include "rak/error_number.h"
#include "torrent/exceptions.h"
#include "utils/instrumentation.h"

namespace torrent {

thread_interrupt::thread_interrupt(int fd) :
  m_poking(false) {
  m_fileDesc = fd;
  get_fd().set_nonblock();
}

thread_interrupt::~thread_interrupt() {
  if (m_fileDesc == -1)
    return;

  ::close(m_fileDesc);
  m_fileDesc = -1;
}

bool
thread_interrupt::poke() {
  if (m_poking.test_and_set())
    return true;

  int result = ::send(m_fileDesc, "a", 1, 0);

  if (result == 0 ||
      (result == -1 && !rak::error_number::current().is_blocked_momentary()))
    throw internal_error("Invalid result writing to thread_interrupt socket.");

  instrumentation_update(INSTRUMENTATION_POLLING_INTERRUPT_POKE, 1);

  return true;
}

thread_interrupt::pair_type
thread_interrupt::create_pair() {
  int fd1, fd2;

  if (!SocketFd::open_socket_pair(fd1, fd2))
    throw internal_error("Could not create socket pair for thread_interrupt: " + std::string(rak::error_number::current().c_str()) + ".");

  thread_interrupt* t1 = new thread_interrupt(fd1);
  thread_interrupt* t2 = new thread_interrupt(fd2);

  t1->m_other = t2;
  t2->m_other = t1;

  return pair_type(t1, t2);
}

void
thread_interrupt::event_read() {
  // This has a race condition where if poked again while processing the polled events it might be
  // missed.
  m_poking.clear();

  char buffer[256];
  int result = ::recv(m_fileDesc, buffer, 256, 0);

  if (result == 0 ||
      (result == -1 && !rak::error_number::current().is_blocked_momentary()))
    throw internal_error("Invalid result reading from thread_interrupt socket.");

  instrumentation_update(INSTRUMENTATION_POLLING_INTERRUPT_READ_EVENT, 1);
}

}
