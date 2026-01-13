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
    if (!socket->m_self_exists)
      throw internal_error("CurlSocket::receive_socket() called on deleted socket");

    if (socket->m_stack == nullptr) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : socket already closed, ignoring", 0);
      return 0;
    }

    if (!socket->is_open()) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : socket not open, ignoring", 0);
      return 0;
    }
  }

  if (what == CURL_POLL_REMOVE) {
    if (socket == nullptr) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : CURL_POLL_REMOVE with null socket", 0);
      return 0;
    }

    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : CURL_POLL_REMOVE received", 0);

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket() CURL_POLL_REMOVE fd mismatch");

    this_thread::poll()->remove_read(socket);
    this_thread::poll()->remove_write(socket);

    // TODO: Figure out if we need to remove error monitoring, and if so we need to add it when reusing fds.
    // this_thread::poll()->remove_error(socket);

    // When libCurl reuses a file descriptor it will pass a nullptr for 'socket', so setting
    // m_easy_handle to nullptr indicates that this socket has been removed and can be reused.
    socket->m_easy_handle = nullptr;

    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running()) {
      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : stack not running, skipping socket creation", 0);
      return 0;
    }

    // TODO: Search for existing socket in stack->fd_to_socket_map(), or create a new one if not existing.
    //
    // Seems libcurl sends up a new fd with NULL user data when it reuses a fd.

    auto itr = stack->fd_to_socket_map().find(fd);

    if (itr == stack->fd_to_socket_map().end()) {
      socket = new CurlSocket(fd, stack, easy_handle);

      stack->fd_to_socket_map().insert({fd, std::unique_ptr<CurlSocket>(socket)});

      curl_multi_assign(stack->handle(), fd, socket);
      curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETDATA, socket);
      curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETFUNCTION, &CurlSocket::close_socket);

      this_thread::poll()->open(socket);
      this_thread::poll()->insert_error(socket);

      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : created new socket", 0);

    } else {
      socket = itr->second.get();

      if (socket->m_easy_handle != nullptr)
        throw internal_error("CurlSocket::receive_socket() reusing existing socket with active easy handle");

      socket->m_easy_handle = easy_handle;
      // this_thread::poll()->insert_error(socket);

      LT_LOG_DEBUG_SOCKET_FD("receive_socket() : reusing existing socket", 0);
    }
  }

  if (socket->m_easy_handle != easy_handle) {
    LT_LOG_DEBUG_SOCKET_FD("receive_socket() : easy handle mismatch", 0);
    throw internal_error("CurlSocket::receive_socket() easy handle mismatch");
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
  if (!socket->is_open()) {
    // Socket error forced us to close it already.
    LT_LOG_DEBUG_SOCKET_FD("close_socket() : socket already closed, ignoring", 0);
    return 0;
  }

  LT_LOG_DEBUG_SOCKET_FD("close_socket() called", 0);

  if (fd != socket->file_descriptor())
    throw internal_error("CurlSocket::close_socket() fd mismatch");

  auto itr = socket->m_stack->fd_to_socket_map().find(fd);

  if (itr == socket->m_stack->fd_to_socket_map().end())
    throw internal_error("CurlSocket::close_socket() could not find fd in fd_to_socket_map()");

  if (itr->second.get() != socket)
    throw internal_error("CurlSocket::close_socket() fd_to_socket_map() socket mismatch");

  socket->close();

  if (::close(fd) != 0)
    throw internal_error("CurlSocket::close_socket() error closing fd");

  // TODO: Add lazy deletion, make sure to include both timers and receive_socket() call counter to
  // avoid wakeup from sleep causing premature deletion.

  socket->m_stack->sockets_to_delete().push_back(std::move(itr->second));
  socket->m_stack->fd_to_socket_map().erase(itr);

  return 0;
}

void
CurlSocket::close() {
  LT_LOG_DEBUG_THIS("close() called", 0);

  if (!is_open())
    return;

  this_thread::poll()->remove_and_close(this);

  m_stack = nullptr;
  m_fileDesc = -1;

  // TODO: Mark CurlSocket for deletion.
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
  LT_LOG_DEBUG_THIS("event_error() called", 0);

  handle_action(CURL_CSELECT_ERR);

  // We are required to close the socket on error, however libcurl may still do callbacks and assume
  // it is open.
  //
  // Since deletion of CurlSocket's are delayed, it's safe to call close() here.
  close();

  // TODO: We might still have pending libcurl callbacks, mark this fd as in error state to avoid issues?

  // TODO: Also, we migth not be getting socket close calls from libcurl after reuse.
}

void
CurlSocket::handle_action(int ev_bitmask) {
  assert(is_open() && "CurlSocket::handle_action(...) is_open()");
  assert(m_stack != nullptr && "CurlSocket::handle_action(...) m_stack != nullptr");

  // Processing might deallocate this CurlSocket.
  auto stack = m_stack;

  int  count{};
  auto code = curl_multi_socket_action(m_stack->handle(), m_fileDesc, ev_bitmask, &count);

  if (code != CURLM_OK)
    throw internal_error("CurlSocket::handle_action(...) error calling curl_multi_socket_action: " + std::string(curl_multi_strerror(code)));

  while (stack->process_done_handle())
    ; // Do nothing.
}

} // namespace torrent::net
