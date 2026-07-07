#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <unistd.h>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"

#include <sys/stat.h>
#include <sys/un.h>

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
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, this, this->file_descriptor(), __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET(log_fmt, ...)                               \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): " log_fmt, socket, socket != nullptr ? socket->file_descriptor() : 0, __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET_FD(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i : " log_fmt, socket, socket != nullptr ? socket->file_descriptor() : 0, fd, __VA_ARGS__)
#define LT_LOG_DEBUG_SOCKET_FD_HANDLE(log_fmt, ...)                            \
  lt_log_print(LOG_CONNECTION_FD, "curl_socket->%p(%i): fd:%i easy_handle:%p : " log_fmt, socket, socket != nullptr ? socket->file_descriptor() : 0, fd, easy_handle, __VA_ARGS__)

#endif

namespace torrent::net {

namespace {

void verify_libcurl_internal_wakeup(int fd);

}

CurlSocket::CurlSocket(CurlStack* stack)
  : m_stack(stack) {
}

CurlSocket::~CurlSocket() {
  assert(!is_open() && "CurlSocket::~CurlSocket() !is_open()");
}

int
CurlSocket::receive_socket(CURL* easy_handle, curl_socket_t fd, int what, CurlStack* stack, CurlSocket* socket) {
  assert(this_thread::thread() == stack->thread());

  if (socket != nullptr && socket->m_self_ptr.expired()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : called on deleted CurlSocket, aborting : %p", socket);
    throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called on deleted CurlSocket");
  }

  if (what == CURL_POLL_REMOVE)
    return handle_poll_remove(easy_handle, fd, stack, socket);

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    socket = handle_poll_new(easy_handle, fd, stack);
  }

  if (!stack->is_running()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket() : CurlStack not running, aborting", 0);
    throw internal_error("CurlSocket::receive_socket(fd:" + std::to_string(fd) + ") called while CurlStack is not running");
  }

  if (what == CURL_POLL_NONE) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("receive_socket(CURL_POLL_NONE) : removing read and write", 0);

    this_thread::poll()->remove_read(socket);
    this_thread::poll()->remove_write(socket);
    return 0;
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
CurlSocket::open_socket(CurlStack *stack, curlsocktype purpose, struct curl_sockaddr *address) {
  assert(this_thread::thread() == stack->thread());

  if (!stack->is_running()) {
    LT_LOG_DEBUG("open_socket() : curl stack not running, aborting", 0);
    return CURL_SOCKET_BAD;
  }

  if (purpose != CURLSOCKTYPE_IPCXN) {
    LT_LOG_DEBUG("open_socket() : unsupported socket type: %i, aborting", purpose);
    throw internal_error("CurlSocket::open_socket() unsupported socket type: " + std::to_string(purpose));
  }

  auto socket_ptr = std::make_shared<CurlSocket>(stack);
  auto socket     = socket_ptr.get();

  socket->m_self_ptr = socket_ptr;

  auto open_func = [socket, address]() {
      int socktype = address->socktype;

#ifdef SOCK_CLOEXEC
      socktype |= SOCK_CLOEXEC;
#endif

      int fd = ::socket(address->family, socktype, address->protocol);

      if (fd == -1) {
        LT_LOG_DEBUG("open_socket() : error creating socket: %s", std::strerror(errno));
        return -1;
      }

      socket->set_file_descriptor(fd);

      return fd;
    };

  runtime::socket_manager()->open_event_or_throw(socket, runtime::category_http, open_func);

  this_thread::poll()->open(socket);

  auto [itr, inserted] = stack->socket_map()->try_emplace(socket->file_descriptor(), std::move(socket_ptr));

  if (!inserted) {
    LT_LOG_DEBUG_SOCKET("open_socket() : fd:%i : socket already exists in stack", socket->file_descriptor());
    throw internal_error("CurlSocket::open_socket() : fd:" + std::to_string(socket->file_descriptor()) + " : socket already exists in stack");
  }

  LT_LOG_DEBUG_SOCKET("open_socket() : socket opened", 0);

  return itr->first;
}

// When receive_socket() is called with CURL_POLL_REMOVE, we call CurlSocket::close() which
// deregisters this callback. (INVALID COMMENT)

int
CurlSocket::close_socket(CurlStack* stack, curl_socket_t fd) {
  assert(this_thread::thread() == stack->thread());

  auto itr = stack->socket_map()->find(fd);

  if (itr == stack->socket_map()->end()) {
    LT_LOG_DEBUG("close_socket() : fd:%i : socket not found in stack, aborting", 0);
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): socket not found in stack");
  }

  auto* socket = itr->second.get();

  LT_LOG_DEBUG_SOCKET("close_socket()", 0);

  if (socket->m_self_ptr.expired()) {
    LT_LOG_DEBUG_SOCKET_FD("close_socket() : called on deleted CurlSocket, aborting", 0);
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + ") called on deleted CurlSocket");
  }

  if (!socket->is_polling())
    throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): socket not in poll");

  if (fd != socket->file_descriptor())
    throw internal_error("CurlSocket::close_socket(): fd mismatch : curl:" + std::to_string(fd) + " self:" + std::to_string(socket->file_descriptor()));

  curl_multi_assign(stack->handle(), fd, nullptr);

  runtime::socket_manager()->close_event_or_throw(socket, [&]() {
      this_thread::poll()->remove_and_close(socket);

      if (::close(fd) == -1) {
        LT_LOG_DEBUG_SOCKET_FD("close_socket() : error closing socket: %s", system::errno_enum(errno));
        throw internal_error("CurlSocket::close_socket(fd:" + std::to_string(fd) + "): error closing socket: " + system::errno_enum_str(errno));
      }

      socket->reset_file_descriptor();
    });

  socket->clear_and_erase_self(itr);
  return 0;
}

CurlSocket*
CurlSocket::handle_poll_new(CURL* easy_handle, curl_socket_t fd, CurlStack* stack) {
  auto [itr, inserted] = stack->socket_map()->try_emplace(fd, nullptr);

  if (inserted) {
    verify_libcurl_internal_wakeup(fd);

    auto socket_ptr = std::make_shared<CurlSocket>(stack);
    auto socket     = socket_ptr.get();

    socket->m_easy_handle   = easy_handle;
    socket->m_curl_internal = true;
    socket->m_self_ptr      = socket_ptr;

    socket->set_file_descriptor(fd);

    LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_new() : internal libcurl socket detected", 0);

    runtime::socket_manager()->register_event_or_throw(socket, runtime::category_http, [&]() {
        this_thread::poll()->open(socket);
      });

    curl_multi_assign(stack->handle(), fd, socket);

    itr->second = std::move(socket_ptr);
    return socket;
  }

  auto socket = itr->second.get();

  if (socket->m_easy_handle != nullptr) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_new() : existing CurlSocket easy_handle not null, aborting", 0);
    throw internal_error("CurlSocket::handle_poll_new(fd:" + std::to_string(fd) + ") existing CurlSocket easy_handle not null");
  }

  socket->m_easy_handle = easy_handle;

  curl_multi_assign(stack->handle(), fd, socket);

  LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_new() : found correctly initialized socket", 0);

  return socket;
}

int
CurlSocket::handle_poll_remove(CURL* easy_handle, curl_socket_t fd, CurlStack* stack, CurlSocket* socket) {
  // When libcurl closes a socket in the idle connection poll, it calls receive_socket() with a
  // null socket.

  if (socket == nullptr) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("CurlSocket::handle_poll_remove(CURL_POLL_REMOVE) : called with null socket, ignoring", 0);
    return 0;
  }

  if (!socket->is_open() || !socket->is_polling()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_remove(CURL_POLL_REMOVE) : socket already closed, aborting", 0);
    throw internal_error("CurlSocket::handle_poll_remove(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE called on closed socket");
  }

  if (socket->file_descriptor() != fd)
    throw internal_error("CurlSocket::handle_poll_remove(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE fd mismatch");

  auto itr = stack->socket_map()->find(fd);

  if (itr == stack->socket_map()->end()) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_remove(CURL_POLL_REMOVE) : socket not found in stack, aborting", 0);
    throw internal_error("CurlSocket::handle_poll_remove(fd:" + std::to_string(fd) + ") CURL_POLL_REMOVE socket not found in stack");
  }

  // LibCurl doesn't use open/close_socket socketpairs, so we only do poll events for them.
  if (socket->m_curl_internal) {
    LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_remove(CURL_POLL_REMOVE) : internal curl socket, removing from poll", 0);

    runtime::socket_manager()->unregister_event_or_throw(socket, [&]() {
        this_thread::poll()->remove_and_close(socket);
      });

    curl_multi_assign(stack->handle(), fd, nullptr);

    socket->clear_and_erase_self(itr);
    return 0;
  }

  LT_LOG_DEBUG_SOCKET_FD_HANDLE("handle_poll_remove(CURL_POLL_REMOVE) : removing from read/write polling", 0);

  this_thread::poll()->remove_read(socket);
  this_thread::poll()->remove_write(socket);
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
  LT_LOG_DEBUG_THIS("event_error()", 0);

  auto weak_self = m_self_ptr;

  handle_action(CURL_CSELECT_ERR);

  // This shouldn't happen, or might be a bug / unexpected behavior in libcurl.
  if (!weak_self.expired()) {
    LT_LOG_DEBUG_THIS("event_error() : self still exists : is_internal:%i", m_curl_internal);
    throw internal_error("CurlSocket::event_error() self still exists : " +
                         std::to_string(file_descriptor()) + " : is_internal:" + std::to_string(m_curl_internal));
  }
}

void
CurlSocket::handle_action(int ev_bitmask) {
  assert(is_open() && "CurlSocket::handle_action() is_open()");
  assert(m_stack != nullptr && "CurlSocket::handle_action() m_stack != nullptr");

  // Processing might deallocate this CurlSocket.
  auto stack = m_stack;

  CurlSocket::handle_action_simple(stack, file_descriptor(), ev_bitmask);

  //
  // TODO: Use Thread::call_events() to process this: ?
  //
  while (stack->process_done_handle())
    ; // Do nothing.
}

void
CurlSocket::handle_action_simple(CurlStack* stack, int fd, int ev_bitmask) {
  int  count{};
  auto code = curl_multi_socket_action(stack->handle(), fd, ev_bitmask, &count);

  if (code != CURLM_OK)
    throw internal_error("CurlSocket::handle_action_simple(...) error calling curl_multi_socket_action: " + std::string(curl_multi_strerror(code)));
}

void
CurlSocket::clear_and_erase_self(CurlStack::socket_map_type::iterator itr) {
  auto socket_map = m_stack->socket_map();

  m_stack       = nullptr;
  m_easy_handle = nullptr;

  reset_file_descriptor();

  socket_map->erase(itr);
}

namespace {

void
verify_libcurl_internal_wakeup(int fd) {
  struct stat sb;

  if (fstat(fd, &sb) == -1) {
    LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fstat failed: %s", fd, system::errno_enum(errno));
    throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fstat failed: " + system::errno_enum_str(errno));
  }

  // Catch pipe and fifo (wakeup_pipe)
  if (S_ISFIFO(sb.st_mode))
    return;

  if (S_ISSOCK(sb.st_mode)) {
    auto local_addr = fd_get_socket_name(fd);

    if (local_addr == nullptr) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : getsockname failed: %s", fd, system::errno_enum(errno));
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): getsockname failed: " + system::errno_enum_str(errno));
    }

    auto peer_addr  = fd_get_peer_name(fd);

    if (peer_addr == nullptr) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : getpeername failed: %s", fd, system::errno_enum(errno));
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): getpeername failed: " + system::errno_enum_str(errno));
    }

    // Catch standard socketpairs (wakeup_socketpair)
    if (local_addr->sa_family == AF_UNIX && peer_addr->sa_family == AF_UNIX) {
      auto *un_local = reinterpret_cast<const sockaddr_un*>(local_addr.get());
      auto *un_peer  = reinterpret_cast<const sockaddr_un*>(peer_addr.get());

      if (strlen(un_local->sun_path) == 0 && strlen(un_peer->sun_path) == 0)
        return;

      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd appears to be a unix socket, but local/peer addresses are not anonymous", 0);
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd appears to be a unix socket, but local/peer addresses are not anonymous");
    }

    int socket_type{};

    if (!fd_get_type(fd, &socket_type)) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : getsockopt(SO_TYPE) failed: %s", fd, system::errno_enum(errno));
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): getsockopt(SO_TYPE) failed: " + system::errno_enum_str(errno));
    }

    // Catch DNS and various other UDP sockets
    if (socket_type == SOCK_DGRAM)
      return;

    if (socket_type != SOCK_STREAM) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : unexpected socket type: %i", fd, socket_type);
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): unexpected socket type: " + std::to_string(socket_type));
    }

    // Catch loopback sockets (wakeup_inet)
    if (sap_is_inet_inet6(local_addr) && sap_is_inet_inet6(peer_addr)) {

      if (!sap_is_loopback(local_addr) || !sap_is_loopback(peer_addr)) {
        LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd appears to be an inet/inet6 socket, but local/peer addresses are not loopback : %s : %s",
                     fd, sap_pretty_str(local_addr).c_str(), sap_pretty_str(peer_addr).c_str());
        throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd appears to be an inet/inet6 socket, but local/peer addresses are not loopback");
      }

      return;
    }

    LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd appears to be a socket, but not a valid libcurl internal wakeup socket", fd);
    throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd appears to be a socket, but not a valid libcurl internal wakeup socket");
  }

  // Linux eventfd (Only probe if it's an anonymous inode, NOT a socket)
  if (!S_ISREG(sb.st_mode) && !S_ISDIR(sb.st_mode)) {
    bool is_nonblock{};

    if (!fd_get_nonblock(fd, &is_nonblock)) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd_get_nonblock failed: %s", fd, system::errno_enum(errno));
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd_get_nonblock failed: " + system::errno_enum_str(errno));
    }

    if (!is_nonblock) {
      LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd is blocking, expected non-blocking", fd);
      throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd is blocking, expected non-blocking");
    }

    char dummy[7] = {0};

    // Passing 7 bytes to an eventfd instantly returns EINVAL without altering state.
    // A regular file or unexpected stream might accept it, but it filters out eventfd perfectly.
    if (write(fd, dummy, 7) == -1 && errno == EINVAL)
      return;
  }

  LT_LOG_DEBUG("verify_libcurl_internal_wakeup(fd:%i) : fd does not appear to be a valid libcurl internal wakeup socket", fd);
  throw internal_error("verify_libcurl_internal_wakeup(fd:" + std::to_string(fd) + "): fd does not appear to be a valid libcurl internal wakeup socket");
}

} // namespace anonymous

} // namespace torrent::net

