#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <unistd.h>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"

#if 0

#define LT_LOG_DEBUG(log_fmt, ...)
#define LT_LOG_DEBUG_THIS(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)
#define LT_LOG_DEBUG_SOCKET_FD_HANDLE(log_fmt, ...)

#else

#define LT_LOG_DEBUG(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket: " log_fmt, __VA_ARGS__)
#define LT_LOG_DEBUG_THIS(log_fmt, ...)                                 \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, this, this->m_fileDesc, __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)                               \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i : " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, fd, __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET_FD_HANDLE(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i easy_handle:%p : " log_fmt, socket, socket != nullptr ? socket->m_fileDesc : 0, fd, easy_handle, __VA_ARGS__)

#endif

// Use getsockopt(SO_TYPE), SOCK_STREAM, and getpeername() / getsockname() to verify the socket is
// the same as when marking inactive.

namespace torrent::net {

CurlSocket::CurlSocket(int fd, CurlStack* stack, CURL* easy_handle)
  : m_stack(stack),
    m_easy_handle(easy_handle) {

  set_file_descriptor(fd);
}

CurlSocket::CurlSocket(CurlStack* stack)
  : m_stack(stack) {
}

CurlSocket::~CurlSocket() {
  assert(!is_open() && "CurlSocket::~CurlSocket() !is_open()");

  m_self_exists = false;
}

int
CurlSocket::receive_socket(CURL* easy_handle, curl_socket_t fd, int what, CurlStack* stack, CurlSocket* socket) {
  assert(this_thread::thread() == stack->thread());

  // We always return 0, even when stack is not running, as we depend on these calls to delete
  // sockets.

  if (socket != nullptr) {
    if (!socket->m_self_exists) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : called on deleted CurlSocket, aborting", 0);
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called on deleted CurlSocket");
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
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE called on closed socket");
    }

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE fd mismatch");

    if (!socket->m_properly_opened) {
      // This is likely a socketpair created by libcurl directly, so we assume libcurl also handles
      // closing it.
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : socket never properly opened, removing from poll", 0);

      runtime::socket_manager()->unregister_event_or_throw(socket, [&]() {
          this_thread::poll()->remove_and_close(socket);
        });

      socket->clear_and_erase_self_or_throw();
      return 0;
    }

    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : removing from read/write polling", 0);

    // TODO: Check m_uninterested flag?

    auto func = [socket]() {
        this_thread::poll()->remove_read(socket);
        this_thread::poll()->remove_write(socket);

        socket->m_easy_handle  = nullptr;
        socket->m_uninterested = true;
      };

    auto on_reuse = [socket]() {
        this_thread::poll()->remove_and_close(socket);
        socket->clear_and_erase_self_or_throw();
      };

    if (!runtime::socket_manager()->mark_stream_event_inactive(socket, func, on_reuse)) {
      // Socket was closed by the kernel, or reused.
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_REMOVE) : socket was not connected, returning error", 0);
      return -1;
    }

    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    auto itr = stack->socket_map()->find(fd);

    if (itr == stack->socket_map()->end()) {
      socket = new CurlSocket(fd, stack, easy_handle);
      socket->m_properly_opened = false;

      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : unexpected fd encountered, creating new (not properly opened) CurlSocket", 0);

      runtime::socket_manager()->register_event_or_throw(socket, [&]() {
          this_thread::poll()->open(socket);
          this_thread::poll()->insert_error(socket);
        });

      stack->socket_map()->emplace(fd, std::unique_ptr<CurlSocket>(socket));

      curl_multi_assign(stack->handle(), fd, socket);

    } else {
      // LibCurl is reusing the fd for a new connection, and as such passed us null socketp.

      if (itr->second->m_easy_handle != nullptr) {
        LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing CurlSocket easy_handle not null, aborting", 0);
        throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") existing CurlSocket easy_handle not null");
      }

      socket = itr->second.get();
      socket->m_easy_handle = easy_handle;

      curl_multi_assign(stack->handle(), fd, socket);

      if (!socket->update_and_verify_socket_address()) {
        LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing socket address mismatch, aborting", 0);
        throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + "): existing socket address mismatch");
      }

      if (!socket->update_and_verify_peer_address()) {
        LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing peer address mismatch, aborting", 0);
        throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + "): existing peer address mismatch");
      }

      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : existing socket found in map : socket:%s peer:%s",
                                    sa_pretty_str(socket->socket_address()).c_str(), sa_pretty_str(socket->peer_address()).c_str());
    }
  }

  if (!stack->is_running()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : CurlStack not running, aborting", 0);
    throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called while CurlStack is not running");
  }

  if (what != CURL_POLL_NONE && socket->m_uninterested) {
    // TODO: Do this in a func protected by SocketManager lock?
    if (!runtime::socket_manager()->mark_event_active_or_fail(socket)) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : socket fd was reused, aborting", 0);

      // TODO: This can cause libcurl to call close_socket() on a reused fd?
      // TODO: Need to check proper opened and remove from poll?

      // TODO: This fd has been reused, no need to do anything else.
      socket->clear_and_erase_self_or_throw();
      return -1;
    }

    socket->m_uninterested = false;

    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : marking socket as interested", 0);
  }

  // TODO: If this is a 'properly opened' socket, update and verify addresses.
  // TODO: This should be done other places also.
  // TODO: Should we also always check SocketManager?

  if (what == CURL_POLL_NONE) {
    // Handle uninterested sockets before checking addresses, as the server tends to close
    // connections right after completing requests. (which invalidates the peer name)
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_NONE) : removing read and write", 0);

    socket->m_uninterested = true;

    auto func = [socket]() {
        this_thread::poll()->remove_read(socket);
        this_thread::poll()->remove_write(socket);
      };

    auto on_reuse = [socket]() {
        this_thread::poll()->remove_and_close(socket);
        socket->clear_and_erase_self_or_throw();
      };

    if (!runtime::socket_manager()->mark_stream_event_inactive(socket, func, on_reuse)) {
      // Not connected...
      // throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") CURL_POLL_NONE: socket was not connected");
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_NONE) : socket was not connected, returning error", 0);
      return -1;
    }

    return 0;
  }

  if (socket->m_properly_opened) {
    if (!socket->update_and_verify_socket_address()) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : socket address mismatch on properly opened socket, returning error", 0);
      return -1;
    }

    if (!socket->update_and_verify_peer_address()) {
      LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : peer address mismatch on properly opened socket, returning error", 0);
      return -1;
    }
  }

  switch (what) {
  case CURL_POLL_IN:
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_IN) : inserting read, removing write", 0);
    this_thread::poll()->insert_read(socket);
    this_thread::poll()->remove_write(socket);
    break;

  case CURL_POLL_OUT:
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_OUT) : inserting write, removing read", 0);
    this_thread::poll()->insert_write(socket);
    this_thread::poll()->remove_read(socket);
    break;

  case CURL_POLL_INOUT:
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_INOUT) : inserting read and write", 0);
    this_thread::poll()->insert_read(socket);
    this_thread::poll()->insert_write(socket);
    break;

  default:
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : unknown what value: %i, aborting", 0);
    throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + "): unknown what value: " + std::to_string(what));
  }

  return 0;
}

// This is not called when libcurl creates socketpairs, yet it expects us to handle them in receive_socket().
curl_socket_t
CurlSocket::open_socket(CurlStack *stack, [[maybe_unused]] curlsocktype purpose, struct curl_sockaddr *address) {
  assert(this_thread::thread() == stack->thread());

  if (!stack->is_running()) {
    LT_LOG_DEBUG("open_socket() : curl stack not running, aborting", 0);
    return CURL_SOCKET_BAD;
  }

  auto event = std::make_unique<CurlSocket>(stack);
  event->m_properly_opened = true;

  auto open_func = [&]() {
      int fd = ::socket(address->family, address->socktype, address->protocol);

      if (fd == -1) {
        LT_LOG_DEBUG("open_socket() : error creating socket: %s", std::strerror(errno));
        return -1;
      }

      // TODO: Fail if fd is already in use, especially if it is m_properly_opened.

      event->set_file_descriptor(fd);

      // TODO: We should add this to uninterested sockets until libcurl tells us to poll.

      this_thread::poll()->open(event.get());
      this_thread::poll()->insert_error(event.get());

      return fd;
    };

  auto cleanup_func = [&]() {
      if (!event->is_open())
        return;

      this_thread::poll()->remove_and_close(event.get());

      if (::close(event->file_descriptor()) == -1) {
        LT_LOG_DEBUG("open_socket() : error closing socket during cleanup: %s", std::strerror(errno));
        throw internal_error("CurlSocket::open_socket(): error closing socket during cleanup: " + std::string(std::strerror(errno)));
      }

      LT_LOG_DEBUG("open_socket() : socket manager requested cleanup: fd:%i", event->file_descriptor());
      event->set_file_descriptor(-1);
    };

  bool result = runtime::socket_manager()->open_event_or_cleanup(event.get(), open_func, cleanup_func);

  if (!result) {
    LT_LOG_DEBUG("open_socket() : error creating socket: %s", std::strerror(errno));
    return CURL_SOCKET_BAD;
  }

  // Add to stack map if not already present.

  auto fd  = event->file_descriptor();
  auto itr = stack->socket_map()->find(fd);

  if (itr != stack->socket_map()->end()) {
    LT_LOG_DEBUG("open_socket() : fd:%i : socket already exists in stack", fd);

    // Shouldn't happen, but close the new socket and return the existing one.
    // runtime::socket_manager()->close_event(event.get());
    // return itr->second->file_descriptor();
    throw internal_error("CurlSocket::open_socket() : fd:%i : socket already exists in stack");
  }

  LT_LOG_DEBUG("open_socket() : socket opened: fd:%i", fd);

  stack->socket_map()->emplace(fd, std::move(event));

  return fd;
}

// When receive_socket() is called with CURL_POLL_REMOVE, we call CurlSocket::close() which
// deregisters this callback.
int
CurlSocket::close_socket(CurlStack* stack, curl_socket_t fd) {
  assert(this_thread::thread() == stack->thread());

  auto itr = stack->socket_map()->find(fd);

  // Socket won't exist if the only thing it called was receive_socket(CURL_POLL_REMOVE).
  //
  // LibCurl is randomly closing / not closing sockets that receive errors, so we need to handle it
  // in a graceful way. (Which isn't really strictly correct)
  if (itr == stack->socket_map()->end()) {

    // TODO: This needs to be changed to keep track of properly opened sockets. (it tries to close fd:0!!!)

    // bool result = runtime::socket_manager()->execute_if_not_present(fd, [&]() {
    //     // TODO: This can cause issues if the fd was by something that doesn't register with SocketManager.

    //     // TODO: Rewrite this to check fd's socket/peer address to verify it is the same socket.
    //     //
    //     // This requires not erasing properly opened CurlSockets on error, etc?

    //     ::close(fd);
    //   });

    // replace with execute_if_matches

    // if (result) {
    //   LT_LOG_DEBUG("close_socket() : fd:%i : socket not found in stack : closed directly", 0);
    // } else {
    //   LT_LOG_DEBUG("close_socket() : fd:%i : socket not found in stack, but is in socket manager : assuming reuse", 0);
    // }

    LT_LOG_DEBUG("close_socket() : fd:%i : socket not found in stack, skipping", 0);
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): socket not found in stack");

    // return 0;
  }

  auto* socket = itr->second.get();

  LT_LOG_DEBUG_SOCKET("close_socket()", 0);

  if (!socket->is_polling())
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): socket not in poll");

  if (fd != socket->file_descriptor())
    throw internal_error("CurlSocket::close_socket(): fd mismatch : curl:" + std::to_string(fd) + " self:" + std::to_string(socket->file_descriptor()));

  curl_multi_assign(stack->handle(), fd, nullptr);

  // TODO: Check if this is still the same socket?

  runtime::socket_manager()->close_event_or_throw(socket, [&]() {
      // TODO: Change handling of curl remove event / uninterested to closing socket polling?

      // TODO: This is getting called twice?
      this_thread::poll()->remove_and_close(socket);

      if (::close(fd) == -1) {
        LT_LOG_DEBUG_SOCKET_FD("close_socket() : error closing socket: %s", std::strerror(errno));
      }

      socket->set_file_descriptor(-1);
    });

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
  // if (m_properly_opened) {
  //   LT_LOG_DEBUG_THIS("event_error() : properly opened socket, removing from socket manager", 0);

  //   runtime::socket_manager()->close_event_or_throw(this, [this]() {
  //       this_thread::poll()->remove_and_close(this);

  //       // clear and erase self.
  //     });

  this_thread::poll()->remove_and_close(this);

  curl_multi_assign(m_stack->handle(), file_descriptor(), nullptr);

  handle_action(CURL_CSELECT_ERR);

  if (!m_self_exists) {
    LT_LOG_DEBUG_THIS("event_error() : self deleted during handle_action, aborting", 0);
    throw internal_error("CurlSocket::event_error() self deleted during handle_action.");
  }

  clear_and_erase_self_or_throw();
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

void
CurlSocket::clear_and_erase_self_or_throw() {
  auto socket_map = m_stack->socket_map();
  auto itr        = socket_map->find(m_fileDesc);

  if (itr == socket_map->end())
    throw internal_error("CurlSocket::clear_and_erase_self_or_throw(): socket not found in stack map");

  clear_and_erase_self(itr);
}

} // namespace torrent::net
