#include "config.h"

#include "torrent/system/ipc/control_fd.h"

#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/system/types.h"

namespace torrent::system::ipc {

void
ControlFd::open(int fd) {
  set_file_descriptor(fd);
}

void
ControlFd::close() {
  if (!is_open())
    return;

  if (::close(file_descriptor()) == -1)
    throw internal_error("ControlFd::close() error closing control fd: " + std::string(std::strerror(errno)));

  set_file_descriptor(-1);
}

bool
ControlFd::check_is_alive(int fd) {
  char dummy;
  auto bytes = ::recv(fd, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);

  if (bytes == 0)
    return false;

  if (bytes == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return true;

    if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE)
      return false;

    // throw internal_error("ControlFd::check_is_alive() recv() failed: " + errno_enum_str(errno));
    return false;
  }

  return true;
}

void
ControlFd::wait_is_alive(int fd) {
  while (true) {
    char dummy;
    auto bytes = ::recv(fd, &dummy, 1, MSG_PEEK);

    if (bytes == 0)
      return;

    if (bytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        continue;

      if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE)
        return;

      throw internal_error("ControlFd::wait_is_alive() recv() failed: " + std::string(std::strerror(errno)));
    }
  }
}

void
ControlFd::send_interrupt() {
  char dummy = 0;

  send_message_internal(&dummy, 0);
}

void
ControlFd::send_shutdown_message(bool graceful) {
  static const std::string graceful_msg = "SHUTDOWN:GRACEFUL";
  static const std::string forceful_msg = "SHUTDOWN:FORCEFUL";

  if (graceful)
    send_message_internal(graceful_msg.c_str(), graceful_msg.size());
  else
    send_message_internal(forceful_msg.c_str(), forceful_msg.size());
}

void
ControlFd::send_fatal_error(const char* msg, uint32_t size) {
  send_message_internal(msg, size);
}

void
ControlFd::send_message_internal(const char* msg, uint32_t size) {
  if (!is_open())
    throw internal_error("ControlFd::send_message_internal() called on closed control fd.");

  if (size > max_message_size)
    throw internal_error("ControlFd::send_message_internal() message size exceeds maximum: " + std::to_string(size));

  std::vector<char> send_buf(2 + size);
  uint16_t          size_network_order = htons(size);

  std::memcpy(send_buf.data(), &size_network_order, 2);
  std::memcpy(send_buf.data() + 2, msg, size);

  size_t total_sent = 0;

  while (total_sent < send_buf.size()) {
    auto sent_bytes = ::send(file_descriptor(), send_buf.data() + total_sent, send_buf.size() - total_sent, 0);

    if (sent_bytes == -1) {
      if (errno == EINTR)
        continue;

      if (errno == EAGAIN || errno == EWOULDBLOCK)
        throw internal_error("ControlFd::send_message_internal(): timed out while sending message.");

      throw internal_error("ControlFd::send_message_internal(): failed to send message: " + std::string(std::strerror(errno)));
    }

    total_sent += static_cast<size_t>(sent_bytes);
  }
}

void
ControlFd::event_read() {
  auto read_fn = [this](char* buffer, unsigned int size) {
      unsigned int total_read = 0;

      while (total_read < size) {
#ifdef __linux__
        int flags = 0;
#else
        int flags = MSG_WAITALL;
#endif

        auto result = ::recv(file_descriptor(), buffer + total_read, size - total_read, flags);

        if (result == 0) {
          m_slot_closed(EPIPE);
          return false;
        }

        if (result == -1) {
          if (errno == EINTR)
            continue;

          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (total_read == 0)
              return false;

#ifdef __linux__
            // On Linux, the socket is non-blocking. To prevent a 100% CPU hot-loop while waiting
            // for the payload, throttle with a tiny sleep.
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
#else
            // On macOS/BSD, the socket is blocking. If EAGAIN happens mid-packet, it means the
            // SO_RCVTIMEO has expired. The peer is dead/unresponsive.
            m_slot_closed(EIO);
            return false;
#endif
          }

          m_slot_closed(errno);
          return false;
        }

        total_read += static_cast<unsigned int>(result);
      }

      return true;
    };

  uint16_t size_network_order = 0;

  if (!read_fn(reinterpret_cast<char*>(&size_network_order), 2))
    return;

  char     buffer[max_message_size];
  uint16_t message_size = ntohs(size_network_order);

  if (message_size == 0) {
    m_slot_interrupt();
    return;
  }

  if (message_size > max_message_size) {
    m_slot_closed(EIO);
    return;
  }

  if (!read_fn(buffer, message_size))
    return;

  if (message_size >= 9 && std::strncmp(buffer, "SHUTDOWN:", 9) == 0) {
    if (message_size == 17 && std::strncmp(buffer + 9, "GRACEFUL", 8) == 0)
      return m_slot_shutdown(true);

    if (message_size == 17 && std::strncmp(buffer + 9, "FORCEFUL", 8) == 0)
      return m_slot_shutdown(false);

    throw internal_error("ControlFd::event_read() received invalid shutdown message: " + std::string(buffer, message_size));
  }

  m_slot_message(std::string(buffer, message_size));
}

void
ControlFd::event_write() {
  throw internal_error("ControlFd::event_write() should not be called on control fd.");
}

void
ControlFd::event_error() {
  throw internal_error("ControlFd::event_error() error on control fd: " + std::string(std::strerror(errno)));
}

} // namespace torrent::system::ipc
