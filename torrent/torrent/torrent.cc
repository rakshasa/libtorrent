#include "config.h"

#include "torrent.h"
#include "exceptions.h"
#include "download_main.h"
#include "throttle_control.h"
#include "timer.h"
#include "peer_handshake.h"
#include "net/listen.h"
#include "url/curl_stack.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

int64_t Timer::m_cache;
std::list<std::string> caughtExceptions;

extern CurlStack curlStack;
Listen* listen = NULL;

struct add_socket {
  add_socket(fd_set* s) : fd(0), fds(s) {}

  void operator () (SocketBase* s) {
    if (s->fd() < 0)
      throw internal_error("Tried to poll a negative file descriptor");

    if (fd < s->fd())
      fd = s->fd();

    FD_SET(s->fd(), fds);
  }

  int fd;
  fd_set* fds;
};

struct check_socket_isset {
  check_socket_isset(fd_set* s) : fds(s) {}

  bool operator () (SocketBase* socket) {
    assert(socket != NULL);

    return FD_ISSET(socket->fd(), fds);
  }

  fd_set* fds;
};

// Make sure srandom is properly initialized by the client.
void initialize() {
  if (listen == NULL) {
    listen = new Listen;

    listen->signal_incoming().connect(sigc::ptr_fun3(&PeerHandshake::connect));
  }

  srandom(Timer::current().usec());

  ThrottleControl::global().insert_service(Timer::current(), 0);

  CurlStack::global_init();
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void cleanup() {
  ThrottleControl::global().remove_service();

  for_each<true>(DownloadMain::downloads().begin(), DownloadMain::downloads().end(),
		 delete_on());

  for_each<true>(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		 delete_on());

  CurlStack::global_cleanup();
}

bool listen_open(uint16_t begin, uint16_t end) {
  if (listen == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  return listen->open(begin, end);
}

void listen_close() {
  listen->close();
}

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
void mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd) {
  *maxFd = 0;

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL || maxFd == NULL)
    throw client_error("mark received a NULL pointer");

  if (curlStack.is_busy())
    curlStack.fdset(readSet, writeSet, exceptSet, *maxFd);

  maxFd = std::max(*maxFd, std::for_each(SocketBase::readSockets().begin(),
					 SocketBase::readSockets().end(),
					 add_socket(readSet)).fd);

  maxFd = std::max(*maxFd, std::for_each(SocketBase::writeSockets().begin(),
					 SocketBase::writeSockets().end(),
					 add_socket(writeSet)).fd);

  maxFd = std::max(*maxFd, std::for_each(SocketBase::exceptSockets().begin(),
					 SocketBase::exceptSockets().end(),
					 add_socket(exceptSet)).fd);

  return maxFd;
}

// Do work on the polled file descriptors.
void work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

// Add all downloads to dlist. Make sure it's cleared.
void downloads_list(DList& dlist);

// Make sure you check that it's valid.
Download downloads_find(const std::string& id);

// Returns 0 if it's not open.
uint32_t get_listen_port();

uint32_t get_handshakes_size();

uint64_t get_select_timeout();
uint64_t get_current_time(); // remove

uint32_t get_http_gets();

uint32_t get_root_throttle();
void     set_root_throttle(uint32_t v);

std::string get_library_name();
std::string get_next_message();
