#include "config.h"

#include "torrent/system/ipc/wakeup_fd.h"

#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"

#ifdef USE_EVENT_FD
#include <sys/eventfd.h>
#endif

namespace torrent::system::ipc {

std::pair<int, int>
WakeupFd::create_fd_pair() {
#ifdef USE_EVENT_FD
  int fd = ::eventfd(0, EFD_NONBLOCK);

  if (fd == -1)
    throw internal_error("WakeupFd::create_fd_pair() eventfd failed: " + errno_enum_str(errno));

  return {fd, fd};

#else
  int fd1, fd2;

  fd_open_socket_pair(fd1, fd2);

  if (!fd_set_nonblock(fd1))
    throw internal_error("WakeupFd::create_fd_pair() fd_set_nonblock(fd1) failed: " + errno_enum_str(errno));

  if (!fd_set_nonblock(fd2))
    throw internal_error("WakeupFd::create_fd_pair() fd_set_nonblock(fd2) failed: " + errno_enum_str(errno));

  return {fd1, fd2};
#endif
}

void
WakeupFd::open(std::pair<int, int> fd_pair, bool is_parent) {
  // Close the unused end of the socket pair.

#ifdef USE_EVENT_FD

  if (fd_pair.first != fd_pair.second)
    throw internal_error("WakeupFd::add_to_poll() eventfd should have the same fd for both ends");

  set_file_descriptor(fd_pair.first);

#else

  if (is_parent) {
    set_file_descriptor(fd_pair.first);
    fd_close(fd_pair.second);
  } else {
    set_file_descriptor(fd_pair.second);
    fd_close(fd_pair.first);
  }

#endif

  if (!fd_set_close_on_exec(file_descriptor(), true))
    throw internal_error("WakeupFd::add_to_poll() fd_set_close_on_exec() failed: " + errno_enum_str(errno));
}

void
WakeupFd::close() {
  if (!is_open())
    return;

  if (is_polling()) {
  // runtime::socket_manager()->unregister_event_or_throw(this, [this, poll]() {
    this_thread::poll()->remove_and_close(this);
    // });
  }

  fd_close(file_descriptor());
  reset_file_descriptor();
}

void
WakeupFd::send_interrupt() {
#ifdef USE_EVENT_FD
  uint64_t value{1};
#else
  uint8_t value{1};
#endif

  while (true) {
    switch (::write(file_descriptor(), &value, sizeof(value))) {
    case sizeof(value):
      return;

    case 0:
      throw internal_error("WakeupFd::send_signal() write returned 0: " + this_thread::thread_name_str());

    case -1:
      if (errno == EINTR)
        continue;

      // Only happens if the eventfd counter is at its maximum value, so it's already interrupting.
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

      // Ignore spurious interrupt attempts right before/after threads enter their event loop.
      // if (errno == EBADF && m_safe_fd == -1)
      //   return;

      throw internal_error("WakeupFd::send_signal() write failed: " + this_thread::thread_name_str() + " : " + std::string(std::strerror(errno)));

    default:
      throw internal_error("WakeupFd::send_signal() write returned unexpected value: " + this_thread::thread_name_str());
    }
  }
}

void
WakeupFd::event_read() {
#ifdef USE_EVENT_FD
  uint64_t value;
#else
  uint8_t value;
#endif

  while (true) {
    switch (::read(file_descriptor(), &value, sizeof(value))) {
    case sizeof(value):
      return;

    case 0:
      throw internal_error("WakeupFd::event_read() read returned 0: " + this_thread::thread_name_str());

    case -1:
      if (errno == EINTR)
        continue;

      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

      throw internal_error("WakeupFd::event_read() read failed: " + std::string(std::strerror(errno)));

    default:
      throw internal_error("WakeupFd::event_read() read returned unexpected value: " + this_thread::thread_name_str());
    }
  }
}

void
WakeupFd::event_write() {
  throw internal_error("WakeupFd::event_write() called but WakeupFd does not support writing.");
}

void
WakeupFd::event_error() {
  throw internal_error("WakeupFd::event_error() called but WakeupFd does not support error events.");
}

} // namespace torrent::net
