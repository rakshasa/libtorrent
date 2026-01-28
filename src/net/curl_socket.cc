#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <unistd.h>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/poll.h"
#include "torrent/utils/log.h"

#if 0

#define LT_LOG_DEBUG(log_fmt, ...)
#define LT_LOG_DEBUG_THIS(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET_FD_HANDLE(log_fmt, ...)

#else

#define LT_LOG_DEBUG(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket: " log_fmt, __VA_ARGS__);
#define LT_LOG_DEBUG_THIS(log_fmt, ...)                                 \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, this, this->m_fileDesc, __VA_ARGS__);
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)                               \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, __VA_ARGS__);
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i : " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, fd, __VA_ARGS__);
#define LT_LOG_DEBUG_SOCKET_FD_HANDLE(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i easy_handle:%p : " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, fd, easy_handle, __VA_ARGS__);

#endif

// TODO: Currently kevent doesn't report socket errors when not in read/write mode.
//

// We likely need to either have a timeout task to check if error when in idle connectino poll, or
// set it to read mode always.
//
// HTTP shouldn't be sending any data when idle, so read mode shouldn't be a problem.

namespace torrent::net {

CurlSocket::CurlSocket(int fd, CurlStack* stack, CURL* easy_handle)
  : m_stack(stack),
    m_easy_handle(easy_handle) {
  m_fileDesc = fd;
}

CurlSocket::~CurlSocket() {
  assert(!is_open() && "CurlSocket::~CurlSocket() !is_open()");

  m_self_exists = false;
}

int
CurlSocket::receive_socket(CURL* easy_handle, curl_socket_t fd, int what, CurlStack* stack, CurlSocket* socket) {
  // We always return 0, even when stack is not running, as we depend on these calls to delete
  // sockets.

  if (socket != nullptr) {
    if (!socket->m_self_exists) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : called on deleted CurlSocket, aborting", 0);
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called on deleted CurlSocket.");
    }
  }

  if (what == CURL_POLL_REMOVE) {
    // When libcurl closes a socket in the idle connection poll, it calls receive_socket() with a
    // null socket.

    if (socket == nullptr) {
      // Assume libcurl calls this before closing the fd if it is in the idle connection pool.
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : null socket", 0);
      return 0;
    }

    if (!socket->is_open() || !socket->is_polling()) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : socket already closed, aborting", 0);
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE called on closed socket.");
    }

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE fd mismatch.");

    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : removing from read/write polling", 0);

    this_thread::poll()->remove_read(socket);
    this_thread::poll()->remove_write(socket);

    socket->m_easy_handle = nullptr;

    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    auto itr = stack->socket_map()->find(fd);

    if (itr == stack->socket_map()->end()) {
      socket = new CurlSocket(fd, stack, easy_handle);

      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : new socket created and added to poll", 0);

      curl_multi_assign(stack->handle(), fd, socket);

      this_thread::poll()->open(socket);
      this_thread::poll()->insert_error(socket);

      stack->socket_map()->emplace(fd, std::unique_ptr<CurlSocket>(socket));

    } else {
      if (itr->second->m_easy_handle != nullptr) {
        LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing CurlSocket easy_handle not null, aborting", 0);
        throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") existing CurlSocket easy_handle not null.");
      }

      socket = itr->second.get();
      socket->m_easy_handle = easy_handle;

      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing socket found in map", 0);

      curl_multi_assign(stack->handle(), fd, socket);
    }
  }

  if (!stack->is_running()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : CurlStack not running, aborting", 0);
    throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called while CurlStack is not running.");
  }

  // TODO: Do inserts before removes to ensure we never stay without read or write when switching modes.
  // TODO: Set as inactive on CURL_POLL_NONE.

  if (what == CURL_POLL_NONE || what == CURL_POLL_OUT) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : removing read", 0);
    this_thread::poll()->remove_read(socket);
  } else {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : inserting read", 0);
    this_thread::poll()->insert_read(socket);
  }

  if (what == CURL_POLL_NONE || what == CURL_POLL_IN) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : removing write", 0);
    this_thread::poll()->remove_write(socket);
  } else {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : inserting write", 0);
    this_thread::poll()->insert_write(socket);
  }

  return 0;
}

curl_socket_t
CurlSocket::open_socket(CurlStack *stack, [[maybe_unused]] curlsocktype purpose, struct curl_sockaddr *address) {
  if (!stack->is_running())
    return CURL_SOCKET_BAD;

  int fd = ::socket(address->family, address->socktype, address->protocol);

  if (fd == -1) {
    LT_LOG_DEBUG("CurlSocket::open_socket() : error creating socket: %s", std::strerror(errno));
    return CURL_SOCKET_BAD;
  }

  LT_LOG_DEBUG("open_socket() : new socket created : fd:%i", fd);

  return fd;
}

// When receive_socket() is called with CURL_POLL_REMOVE, we call CurlSocket::close() which
// deregisters this callback.
int
CurlSocket::close_socket(CurlStack* stack, curl_socket_t fd) {
  auto itr = stack->socket_map()->find(fd);

  // Socket won't exist if the only thing it called was receive_socket(CURL_POLL_REMOVE).
  if (itr == stack->socket_map()->end()) {
    LT_LOG_DEBUG("close_socket(fd:%i) : socket not found in stack map", fd);

    if (::close(fd) != 0)
      throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): error closing socket: " + std::string(std::strerror(errno)));

    return 0;
  }

  auto* socket = itr->second.get();

  LT_LOG_DEBUG_SOCKET("close_socket()", 0);

  if (!socket->is_polling())
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): socket not in poll");

  if (fd != socket->file_descriptor())
    throw internal_error("CurlSocket::close_socket(): fd mismatch : curl:" + std::to_string(fd) + " self:" + std::to_string(socket->file_descriptor()));

  this_thread::poll()->remove_and_close(socket);
  curl_multi_assign(stack->handle(), fd, nullptr);

  if (::close(fd) != 0)
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): error closing socket: " + std::string(std::strerror(errno)));

  socket->clear_and_erase_self(itr);
  return 0;
}

void
CurlSocket::event_read() {
  handle_action(CURL_CSELECT_IN);
}

void
CurlSocket::event_write() {
  handle_action(CURL_CSELECT_OUT);
}

// TODO: We need to figure out how to tell libcurl to not close an fd when it has been reused.
//
// Perhaps we just pretend it is open while in idle conection poll, and pretend to add it to read
// just to get the CLOSEFUNCTION callback?
//
// Also, verify if reporting errors to libcurl actually closes the fd, doesn't seem like it should?
//
// Think it should be calling the callback.

void
CurlSocket::event_error() {
  LT_LOG_DEBUG_THIS("event_error()", 0);

  // LibCurl will close the socket, so remove it from polling prior to passing the error event.
  this_thread::poll()->remove_and_close(this);
  curl_multi_assign(m_stack->handle(), file_descriptor(), nullptr);

  handle_action(CURL_CSELECT_ERR);

  if (!m_self_exists) {
    LT_LOG_DEBUG_THIS("event_error() : self deleted during handle_action, aborting", 0);
    throw internal_error("CurlSocket::event_error() self deleted during handle_action.");
  }

  auto itr = m_stack->socket_map()->find(m_fileDesc);

  if (itr == m_stack->socket_map()->end()) {
    LT_LOG_DEBUG_THIS("event_error() : socket not found in stack map, aborting", 0);
    throw internal_error("CurlSocket::event_error() socket not found in stack map.");
  }

  clear_and_erase_self(itr);
}

void
CurlSocket::handle_action(int ev_bitmask) {
  assert(is_open() && "CurlSocket::handle_action() is_open()");
  assert(m_stack != nullptr && "CurlSocket::handle_action() m_stack != nullptr");

  // Processing might deallocate this CurlSocket.
  int  count{};
  auto stack = m_stack;
  auto code  = curl_multi_socket_action(m_stack->handle(), m_fileDesc, ev_bitmask, &count);

  if (code != CURLM_OK)
    throw internal_error("CurlSocket::handle_action(...) error calling curl_multi_socket_action: " + std::string(curl_multi_strerror(code)));

  //
  // TODO: Use Thread::call_events() to process this: ?
  //

  while (stack->process_done_handle())
    ; // Do nothing.
}

void
CurlSocket::clear_and_erase_self(CurlStack::socket_map_type::iterator itr) {
  auto socket_map = m_stack->socket_map();

  m_fileDesc    = -1;
  m_stack       = nullptr;
  m_easy_handle = nullptr;

  socket_map->erase(itr);
}

} // namespace torrent::net
