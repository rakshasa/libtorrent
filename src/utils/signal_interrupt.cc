#include "config.h"

#include "utils/signal_interrupt.h"

#include <cerrno>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "utils/instrumentation.h"

namespace torrent {

SignalInterrupt::SignalInterrupt(int fd) {
  m_fileDesc = fd;

  if (::fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    throw internal_error("Could not set non-blocking mode for SignalInterrupt socket: " + std::string(std::strerror(errno)));

  // Not supported on socketpair on MacOS. (TODO: Check if this is true on other platforms.)
  // if (!fd_set_tcp_nodelay(fd))
  //   throw internal_error("Could not set TCP_NODELAY for SignalInterrupt socket: " + std::string(std::strerror(errno)));
}

SignalInterrupt::~SignalInterrupt() {
  if (m_fileDesc == -1)
    return;

  ::close(m_fileDesc);
  m_fileDesc = -1;
}

SignalInterrupt::pair_type
SignalInterrupt::create_pair() {
  // int fd1, fd2;

  // TODO: Use pipe instead?
  // fd_open_socket_pair(fd1, fd2);
  // fd_open_pipe(fd1, fd2);

  // Open explicitly here to avoid mock fd_* functions interferring with thread interrupts and
  // cached_time access during main thread ctor.
  int fds[2];

  if (::socketpair(AF_LOCAL, SOCK_STREAM, 0, fds) == -1)
    throw internal_error("torrent::fd_open_socket_pair failed: " + std::string(strerror(errno)));

  pair_type result{new SignalInterrupt(fds[0]), new SignalInterrupt(fds[1])};

  result.first->m_other = result.second.get();
  result.second->m_other = result.first.get();

  return result;
}

void
SignalInterrupt::poke() {
  bool expected = false;

  if (!m_other->m_poking.compare_exchange_strong(expected, true))
    return;

  int result = ::send(m_fileDesc, "a", 1, 0);

  if (result == 0)
    throw internal_error("Could not send to SignalInterrupt socket, result is 0.");

  if (result == -1)
    throw internal_error("Could not send to SignalInterrupt socket: " + std::string(std::strerror(errno)));

  instrumentation_update(INSTRUMENTATION_POLLING_INTERRUPT_POKE, 1);
}

const char*
SignalInterrupt::type_name() const {
  return "signal_interrupt";
}

void
SignalInterrupt::event_read() {
  char buffer[256];

  int result = ::recv(m_fileDesc, buffer, 256, 0);

  if (result == 0)
    throw internal_error("SignalInterrupt socket closed.");

  if (result == -1)
    throw internal_error("SignalInterrupt socket error: " + std::string(std::strerror(errno)));

  instrumentation_update(INSTRUMENTATION_POLLING_INTERRUPT_READ_EVENT, 1);

  m_poking = false;
}

void
SignalInterrupt::event_write() {
  throw internal_error("SignalInterrupt::event_write() called, but not expected.");
}

void
SignalInterrupt::event_error() {
  throw internal_error("SignalInterrupt::event_error() called, but not expected.");
}

} // namespace torrent
