#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <unistd.h>
#include <curl/multi.h>

#include "net/curl_stack.h"
#include "torrent/net/poll.h"
#include "torrent/exceptions.h"

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
    if (socket == nullptr)
      throw internal_error("CurlSocket::receive_socket() called with CURL_POLL_REMOVE and null socket.");

    // TODO: This should be handled such that ... all the ai suggestions are bad

    if (socket->m_fileDesc == -1)
      throw internal_error("CurlSocket::receive_socket() CURL_POLL_REMOVE on already closed socket.");

    if (socket->m_fileDesc != fd)
      throw internal_error("CurlSocket::receive_socket() CURL_POLL_REMOVE fd mismatch.");

    socket->close();
    delete socket;

    return 0;
  }

  if (socket == nullptr) {
    if (!stack->is_running())
      return 0;

    socket = new CurlSocket(fd, stack, easy_handle);

    curl_multi_assign(stack->handle(), fd, socket);
    curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETDATA, socket);
    curl_easy_setopt(easy_handle, CURLOPT_CLOSESOCKETFUNCTION, &CurlSocket::close_socket);

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

int
CurlSocket::close_socket(CurlSocket* socket, curl_socket_t fd) {
  if (socket == nullptr)
    throw internal_error("CurlSocket::close_socket() called with null socket.");

  if (fd != socket->m_fileDesc)
    throw internal_error("CurlSocket::close_socket() fd mismatch.");

  socket->close();

  if (::close(fd) != 0)
    throw internal_error("CurlSocket::close_socket() error closing fd.");

  // delete socket;
  return 0;
}

CurlSocket::~CurlSocket() {
  assert(m_fileDesc == -1 && "CurlSocket::~CurlSocket() m_fileDesc != -1.");
}

void
CurlSocket::close() {
  if (m_fileDesc == -1)
    return;

  this_thread::poll()->remove_and_close(this);

  // TODO: This will result in the callback receving a null socket pointer, this means we should have one point of deletion.
  // curl_multi_assign(m_stack->handle(), m_fileDesc, nullptr);
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
