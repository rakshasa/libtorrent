#include "config.h"

#include "curl_socket.h"

#include <cassert>
#include <curl/multi.h>
#include <torrent/poll.h>
#include <torrent/exceptions.h>
#include <torrent/utils/thread.h>

#include "net/curl_stack.h"

namespace torrent::net {

int
CurlSocket::receive_socket([[maybe_unused]] void* easy_handle, curl_socket_t fd, int what, void* userp, void* socketp) {
  CurlStack* stack = (CurlStack*)userp;
  CurlSocket* socket = (CurlSocket*)socketp;

  if (!stack->is_running())
    return 0;

  if (what == CURL_POLL_REMOVE) {
    // We also probably need the special code here as we're not
    // guaranteed that the fd will be closed, afaik.
    if (socket != NULL)
      socket->close();

    // TODO: Consider the possibility that we'll need to set the
    // fd-associated pointer curl holds to NULL.

    delete socket;
    return 0;
  }

  if (socket == NULL) {
    socket = stack->new_socket(fd);
    torrent::this_thread::poll()->open(socket);

    // No interface for libcurl to signal when it's interested in error events.
    // Assume that hence it must always be interested in them.
    torrent::this_thread::poll()->insert_error(socket);
  }

  if (what == CURL_POLL_NONE || what == CURL_POLL_OUT)
    torrent::this_thread::poll()->remove_read(socket);
  else
    torrent::this_thread::poll()->insert_read(socket);

  if (what == CURL_POLL_NONE || what == CURL_POLL_IN)
    torrent::this_thread::poll()->remove_write(socket);
  else
    torrent::this_thread::poll()->insert_write(socket);

  return 0;
}

CurlSocket::~CurlSocket() {
  assert(m_fileDesc == -1 && "CurlSocket::~CurlSocket() m_fileDesc != -1.");
}

void
CurlSocket::close() {
  if (m_fileDesc == -1)
    throw torrent::internal_error("CurlSocket::close() m_fileDesc == -1.");

  torrent::this_thread::poll()->closed(this);
  m_fileDesc = -1;
}

void
CurlSocket::event_read() {
  return m_stack->receive_action(this, CURL_CSELECT_IN);
}

void
CurlSocket::event_write() {
  return m_stack->receive_action(this, CURL_CSELECT_OUT);
}

void
CurlSocket::event_error() {
  return m_stack->receive_action(this, CURL_CSELECT_ERR);
}

}
