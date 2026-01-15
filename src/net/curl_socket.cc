#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <unistd.h>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/poll.h"
#include "torrent/utils/log.h"

#define LT_LOG_DEBUG_THIS(log_fmt, ...) \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, this, this->m_fileDesc, __VA_ARGS__);

//#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...) \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, __VA_ARGS__);

// #define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i : " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, fd, __VA_ARGS__);

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
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : called on deleted CurlSocket, aborting", 0);
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called on deleted CurlSocket.");
    }
  }

  if (what == CURL_POLL_REMOVE) {
    // When libcurl closes a socket in the idle connection poll, it calls receive_socket() with a
    // null socket.

    // TODO: Add to unbound fd's list to close / reuse?
    if (socket == nullptr) {
      // throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called with CURL_POLL_REMOVE and null socket.");

      // Assume libcurl calls this before closing the fd if it is in the idle connection pool.

      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : CURL_POLL_REMOVE with null socket", 0);
      return 0;
    }

    if (!socket->is_open()) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : socket already closed, deleting", 0);
      delete socket;
      return 0;
    }

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE fd mismatch.");

    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : CURL_POLL_REMOVE, removing from poll and deleting", 0);

    // TODO: This is wrong, we don't close the fd.

    // TODO: Currently assuming libcurl keeps track of the fd, so we just remove from polling and
    // delete the socket.

    // TODO: Verify we don't leak fd's, that libcurl assumes it should call CURLOPT_CLOSESOCKETFUNCTION.

    this_thread::poll()->remove_and_close(socket);

    // if (socket->m_easy_handle != nullptr)
    //   curl_easy_setopt(socket->m_easy_handle, CURLOPT_CLOSESOCKETFUNCTION, nullptr);

    socket->m_fileDesc    = -1;
    socket->m_stack       = nullptr;
    socket->m_easy_handle = nullptr;

    delete socket;
    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    socket = new CurlSocket(fd, stack, easy_handle);

    curl_multi_assign(stack->handle(), fd, socket);
    // curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETDATA, socket);
    // curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETFUNCTION, &CurlSocket::close_socket);

    this_thread::poll()->open(socket);
    this_thread::poll()->insert_error(socket);

    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : new socket created and added to poll", 0);
  }

  // TODO: This should be impossible.
  if (!stack->is_running()) {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : stack not running, removing socket from poll", 0);

    this_thread::poll()->remove_read(socket);
    this_thread::poll()->remove_write(socket);
    this_thread::poll()->remove_error(socket);
    return 0;
  }

  if (what == CURL_POLL_NONE || what == CURL_POLL_OUT) {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : removing read", 0);
    this_thread::poll()->remove_read(socket);
  } else {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : inserting read", 0);
    this_thread::poll()->insert_read(socket);
  }

  if (what == CURL_POLL_NONE || what == CURL_POLL_IN) {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : removing write", 0);
    this_thread::poll()->remove_write(socket);
  } else {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : inserting write", 0);
    this_thread::poll()->insert_write(socket);
  }

  return 0;
}

// When receive_socket() is called with CURL_POLL_REMOVE, we call CurlSocket::close() which
// deregisters this callback.
int
CurlSocket::close_socket(CurlSocket* socket, curl_socket_t fd) {
  if (socket == nullptr)
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + ") called with null socket.");

  LT_LOG_DEBUG_SOCKET_FD("close_socket() called", 0);

  if (fd != socket->m_fileDesc)
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + ") fd mismatch.");

  this_thread::poll()->remove_and_close(socket);

  if (::close(fd) != 0)
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + ") error closing socket: " + std::string(std::strerror(errno)));

  socket->m_fileDesc    = -1;
  socket->m_stack       = nullptr;
  socket->m_easy_handle = nullptr;

  // We assume the socket is deleted in receive_socket() after CURL_POLL_REMOVE.
  //
  // TODO: We need to verify that libcurl calls CURL_POLL_REMOVE after this.

  // TODO: Add to a delayed delete list?

  // delete socket;
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

void
CurlSocket::event_error() {
  // LibCurl will close the socket, so remove it from polling prior to passing the error event.
  this_thread::poll()->remove_and_close(this);

  handle_action(CURL_CSELECT_ERR);

  if (!m_self_exists) {
    LT_LOG_DEBUG_THIS("event_error() : self deleted during handle_action, aborting", 0);
    throw internal_error("CurlSocket::event_error(fd:" + std::to_string(m_fileDesc) + ") self deleted during handle_action.");
  }

  m_fileDesc    = -1;
  m_stack       = nullptr;
  m_easy_handle = nullptr;

  delete this;
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

} // namespace torrent::net
