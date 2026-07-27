#include "config.h"

#include "torrent/system/ipc/wakeup_fd.h"

#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"

#ifdef USE_EVENT_FD
#include <sys/eventfd.h>
#endif

namespace torrent::system::ipc {

void
WakeupFd::add_to_poll(int fd) {
  errno = 0;

// #ifdef USE_EPOLL
//   set_file_descriptor(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
// #endif

  set_file_descriptor(fd);

  // runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    // });
}

void
WakeupFd::remove_from_poll(Poll* poll) {
  if (!is_open())
    return;

  runtime::socket_manager()->unregister_event_or_throw(this, [this, poll]() {
      poll->remove_and_close(this);
    });
}

void
WakeupFd::send_signal() {
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
