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

int
CurlSocket::receive_socket(CURL* easy_handle, curl_socket_t fd, int what, CurlStack* stack, CurlSocket* socket) {
  // We always return 0, even when stack is not running, as we depend on these calls to delete
  // sockets.

  if (what == CURL_POLL_REMOVE) {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : CURL_POLL_REMOVE", 0);

    if (socket == nullptr)
      throw internal_error("CurlSocket::receive_socket() called with CURL_POLL_REMOVE and null socket.");

    // LibCurl already called close_socket().
    if (!socket->is_open()) {
      delete socket;
      return 0;
    }

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket() CURL_POLL_REMOVE fd mismatch.");

    socket->close();
    delete socket;

    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running()) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : stack not running, skipping socket creation", 0);
      return 0;
    }

    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : new socket", 0);

    socket = new CurlSocket(fd, stack, easy_handle);

    curl_multi_assign(stack->handle(), fd, socket);
    curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETDATA, socket);
    curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETFUNCTION, &CurlSocket::close_socket);

    this_thread::poll()->open(socket);
    this_thread::poll()->insert_error(socket);
  }

  if (socket->m_easy_handle != easy_handle)
    throw internal_error("CurlSocket::receive_socket() easy handle mismatch.");

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
//
// Note that when using multi/share handles, your callback may get invoked even after the easy
// handle has been cleaned up.

// TODO: We need to handle the case where libcurl reuses file descriptors.
//
// This means that the CurlSocket passed to close_socket() may not be the same as the one we
// initially registered.
//
// So keep a map of fd -> CurlSocket in CurlStack?
//
// Or can we just update the CURLOPT_CLOSESOCKETDATA to the new CurlSocket when it reuses a fd?

int
CurlSocket::close_socket(CurlSocket* socket, curl_socket_t fd) {
  LT_LOG_DEBUG_SOCKET_FD("close_socket() called", 0);

  if (socket == nullptr)
    throw internal_error("CurlSocket::close_socket() called with null socket.");

  if (fd != socket->m_fileDesc)
    throw internal_error("CurlSocket::close_socket() fd mismatch.");

  socket->close();

  if (::close(fd) != 0)
    throw internal_error("CurlSocket::close_socket() error closing fd.");

  // We assume the socket is deleted in receive_socket() after CURL_POLL_REMOVE.
  //
  // TODO: We need to verify that libcurl calls CURL_POLL_REMOVE after this.

  // delete socket;
  return 0;
}

CurlSocket::~CurlSocket() {
  assert(!is_open() && "CurlSocket::~CurlSocket() !is_open().");
}

void
CurlSocket::close() {
  LT_LOG_DEBUG_THIS("close() called", 0);

  if (!is_open())
    return;

  this_thread::poll()->remove_and_close(this);

  // Deregister close callback for when receive_socket() is called with CURL_POLL_REMOVE, as this
  // CurlSocket is deleted.

  // TODO: Don't do this?
  curl_easy_setopt(m_easy_handle, CURLOPT_CLOSESOCKETFUNCTION, nullptr);

  m_stack = nullptr;
  m_fileDesc = -1;
}

void
CurlSocket::event_read() {
  return handle_action(CURL_CSELECT_IN);
}

void
CurlSocket::event_write() {
  return handle_action(CURL_CSELECT_OUT);
}

void
CurlSocket::event_error() {
  LT_LOG_DEBUG_THIS("event_error() called", 0);

  // TODO: This needs to close the socket.

  return handle_action(CURL_CSELECT_ERR);
}

void
CurlSocket::handle_action(int ev_bitmask) {
  assert(is_open() && "CurlSocket::handle_action(...) is_open().");
  assert(m_stack != nullptr && "CurlSocket::handle_action(...) m_stack != nullptr.");

  // Processing might deallocate this CurlSocket.
  auto stack = m_stack;

  int count{};
  CURLMcode code = curl_multi_socket_action(m_stack->handle(), m_fileDesc, ev_bitmask, &count);

  if (code != CURLM_OK)
    throw internal_error("CurlSocket::handle_action(...) error calling curl_multi_socket_action: " + std::string(curl_multi_strerror(code)));

  while (stack->process_done_handle())
    ; // Do nothing.
}

} // namespace torrent::net
