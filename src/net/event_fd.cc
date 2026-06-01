#include "config.h"

#include "net/event_fd.h"

#include <unistd.h>

#ifdef USE_EPOLL
#include <sys/eventfd.h>
#endif

#include "torrent/exceptions.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"

namespace torrent::net {

void
EventFd::add_to_poll() {
  errno = 0;

#ifdef USE_EPOLL
  set_file_descriptor(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));

  m_safe_fd = file_descriptor();
#endif

  if (file_descriptor() == -1)
    throw internal_error("EventFd::add_to_poll() eventfd failed: " + std::string(std::strerror(errno)));

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    });
}

void
EventFd::remove_from_poll(system::Poll* poll) {
  if (!is_open())
    return;

  m_safe_fd = -1;

  runtime::socket_manager()->unregister_event_or_throw(this, [this, poll]() {
      poll->remove_and_close(this);
    });
}

void
EventFd::send_signal() {
  uint64_t value = 1;

  while (true) {
    if (::write(m_safe_fd.load(), &value, sizeof(value)) != sizeof(value)) {
      if (errno == EINTR)
        continue;

      // Only happens if the eventfd counter is at its maximum value, so it's already interrupting.
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;

      // Ignore spurious interrupt attempts right before/after threads enter their event loop.
      if (errno == EBADF)
        return;

      throw internal_error("EventFd::send_signal() write failed: " + this_thread::thread_name_str() + " : " + std::string(std::strerror(errno)));
    }
  };
}

void
EventFd::event_read() {
  uint64_t value;

  if (::read(m_safe_fd.load(), &value, sizeof(value)) != sizeof(value))
    throw internal_error("EventFd::event_read() read failed: " + std::string(std::strerror(errno)));
}

void
EventFd::event_write() {
  throw internal_error("EventFd::event_write() called but EventFd does not support writing.");
}

void
EventFd::event_error() {
  throw internal_error("EventFd::event_error() called but EventFd does not support error events.");
}

} // namespace torrent::net
