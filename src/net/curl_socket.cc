#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/net/poll.h"
#include "torrent/exceptions.h"

namespace torrent::net {

int
CurlSocket::receive_socket([[maybe_unused]] void* easy_handle, curl_socket_t fd, int what, CurlStack* stack, CurlSocket* socket) {
  // We always return 0, even when stack is not running, as we depend on these calls to delete
  // sockets.

  if (what == CURL_POLL_REMOVE) {
    if (socket == nullptr)
      return 0;

    // We also probably need the special code here as we're not
    // guaranteed that the fd will be closed, afaik.
    socket->close();

    delete socket;
    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    // TODO: Assign nullptr here....
    // TODO: Might not be getting removed ? verify fd, etc.

    socket = new CurlSocket(fd, stack);
    curl_multi_assign(stack->handle(), fd, socket);

    this_thread::poll()->open(socket);
    this_thread::poll()->insert_error(socket);
  }

  if (!stack->is_running()) {
    this_thread::poll()->remove_read(socket);
    this_thread::poll()->remove_write(socket);
    this_thread::poll()->remove_error(socket);
    return 0;
  }

  if (what == CURL_POLL_NONE || what == CURL_POLL_OUT)
    this_thread::poll()->remove_read(socket);
  else
    this_thread::poll()->insert_read(socket);

  if (what == CURL_POLL_NONE || what == CURL_POLL_IN)
    this_thread::poll()->remove_write(socket);
  else
    this_thread::poll()->insert_write(socket);

  return 0;
}

CurlSocket::~CurlSocket() {
  assert(m_fileDesc == -1 && "CurlSocket::~CurlSocket() m_fileDesc != -1.");
}

void
CurlSocket::close() {
  if (m_fileDesc == -1)
    throw internal_error("CurlSocket::close() m_fileDesc == -1.");

  this_thread::poll()->remove_and_close(this);

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
  return handle_action(CURL_CSELECT_ERR);
}

void
CurlSocket::handle_action(int ev_bitmask) {
  assert(m_fileDesc != -1 && "CurlSocket::handle_action(...) m_fileDesc != -1.");
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
