#include "config.h"

#include "torrent.h"
#include "exceptions.h"
#include "download_main.h"
#include "throttle_control.h"
#include "timer.h"
#include "peer_handshake.h"
#include "net/listen.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

int64_t Timer::m_cache;
std::list<std::string> caughtExceptions;

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
void
initialize() {
  if (listen == NULL) {
    listen = new Listen;

    listen->signal_incoming().connect(sigc::ptr_fun3(&PeerHandshake::connect));
  }

  srandom(Timer::current().usec());

  ThrottleControl::global().insert_service(Timer::current(), 0);
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  ThrottleControl::global().remove_service();

  for_each<true>(DownloadMain::downloads().begin(), DownloadMain::downloads().end(),
		 delete_on());

  for_each<true>(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		 delete_on());
}

bool
listen_open(uint16_t begin, uint16_t end) {
  if (listen == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  return listen->open(begin, end);
}

void
listen_close() {
  listen->close();
}

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
void
mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd) {
  *maxFd = 0;

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL || maxFd == NULL)
    throw client_error("torrent::mark(...) received a NULL pointer");

  *maxFd = std::max(*maxFd, std::for_each(SocketBase::readSockets().begin(),
                                          SocketBase::readSockets().end(),
                                          add_socket(readSet)).fd);

  *maxFd = std::max(*maxFd, std::for_each(SocketBase::writeSockets().begin(),
                                          SocketBase::writeSockets().end(),
                                          add_socket(writeSet)).fd);
  
  *maxFd = std::max(*maxFd, std::for_each(SocketBase::exceptSockets().begin(),
                                          SocketBase::exceptSockets().end(),
                                          add_socket(exceptSet)).fd);
}

// Do work on the polled file descriptors.
void
work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  // Update the cached time.
  Timer::update();

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL)
    throw client_error("Torrent::work(...) received a NULL pointer");

  // Make sure we don't do read/write on fd's that are in except. This should
  // not be a problem as any except call should remove it from the m_*Set's.

  caughtExceptions.clear();

  // If except is called, make sure you correctly remove us from the poll.
  for_each<true>(SocketBase::exceptSockets().begin(), SocketBase::exceptSockets().end(),
		 if_on(check_socket_isset(exceptSet),
		       call_member(&SocketBase::except)));

  for_each<true>(SocketBase::readSockets().begin(), SocketBase::readSockets().end(),
		 if_on(check_socket_isset(readSet),
		       call_member(&SocketBase::read)));

  for_each<true>(SocketBase::writeSockets().begin(), SocketBase::writeSockets().end(),
		 if_on(check_socket_isset(writeSet),
		       call_member(&SocketBase::write)));

  // TODO: Consider moving before the r/w/e. libsic++ should remove the use of
  // zero timeout stuff to send signal. Better yet, use on both sides, it's cheap.
  Service::perform_service();
}

Download
download_create(std::istream& s) {
    // TODO: Should we clear failed bits?
  s.clear();

  bencode b;

  s >> b;

  if (s.fail())
    // Make it configurable whetever we throw or return .end()?
    throw local_error("Could not parse bencoded torrent");
  
  DownloadMain* download = new DownloadMain(b);

  DownloadMain::downloads().insert(DownloadMain::downloads().end(), download);

  return download;
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  for (DownloadMain::Downloads::iterator itr = DownloadMain::downloads().begin(); itr != DownloadMain::downloads().end(); ++itr)
    dlist.push_back(Download(*itr));
}

// Make sure you check that it's valid.
Download
download_find(const std::string& id) {
  DownloadMain::Downloads::iterator itr = std::find_if(DownloadMain::downloads().begin(),
                                                       DownloadMain::downloads().end(),
                                                       eq(ref(id),
                                                          call_member(call_member(&DownloadMain::state),
                                                                      &DownloadState::hash)));

  return itr != DownloadMain::downloads().end() ? Download(*itr) : Download(NULL);
}

void
download_remove(const std::string& id) {
  DownloadMain::Downloads::iterator itr = std::find_if(DownloadMain::downloads().begin(),
                                                       DownloadMain::downloads().end(),
                                                       eq(ref(id),
                                                          call_member(call_member(&DownloadMain::state),
                                                                      &DownloadState::hash)));

  if (itr == DownloadMain::downloads().end())
    throw client_error("Tried to remove a non-existant download");

  delete *itr;
}

// Throws a local_error of some sort.
int64_t
get(GValue t) {
  switch (t) {
  case LISTEN_PORT:
    return listen->get_port();

  case HANDSHAKES_TOTAL:
    return PeerHandshake::handshakes().size();

  case SHUTDOWN_DONE:
    return std::find_if(DownloadMain::downloads().begin(), DownloadMain::downloads().end(),
			bool_not(call_member(&DownloadMain::isStopped)))
      == DownloadMain::downloads().end();

  case FILES_CHECK_WAIT:
    return Settings::filesCheckWait;

  case DEFAULT_PEERS_MIN:
    return DownloadSettings::global().minPeers;

  case DEFAULT_PEERS_MAX:
    return DownloadSettings::global().maxPeers;

  case DEFAULT_UPLOADS_MAX:
    return DownloadSettings::global().maxUploads;

  case DEFAULT_CHOKE_CYCLE:
    return DownloadSettings::global().chokeCycle;

  case HAS_EXCEPTION:
    return !caughtExceptions.empty();

  case TIME_CURRENT:
    return Timer::current().usec();

  case TIME_SELECT:
    return Service::next_service().usec();

  case THROTTLE_ROOT_CONST_RATE:
    return std::max(ThrottleControl::global().settings(ThrottleControl::SETTINGS_ROOT)->constantRate, 0);

  default:
    throw internal_error("get(GValue) received invalid type");
  }
}

std::string
get(GString t) {
  std::string s;

  switch (t) {
  case LIBRARY_NAME:
    return std::string("LibTorrent") + " " VERSION;

  case POP_EXCEPTION:
    if (caughtExceptions.empty())
      throw internal_error("get(GString) tried to pop an exception from an empty list");

    s = caughtExceptions.front();
    caughtExceptions.pop_front();

    return s;

  default:
    throw internal_error("get(GString) received invalid type");
  }
}

void
set(GValue t, int64_t v) {
  switch (t) {
  case FILES_CHECK_WAIT:
    if (v >= 0 && v < 60 * 1000000)
      Settings::filesCheckWait = v;
    break;

  case DEFAULT_PEERS_MIN:
    if (v > 0 && v < 1000)
      DownloadSettings::global().minPeers = v;
    break;

  case DEFAULT_PEERS_MAX:
    if (v > 0 && v < 1000)
      DownloadSettings::global().maxPeers = v;
    break;

  case DEFAULT_UPLOADS_MAX:
    if (v >= 0 && v < 1000)
      DownloadSettings::global().maxUploads = v;
    break;

  case DEFAULT_CHOKE_CYCLE:
    if (v > 10 * 1000000 && v < 3600 * 1000000)
      DownloadSettings::global().chokeCycle = v;
    break;

  case THROTTLE_ROOT_CONST_RATE:
    ThrottleControl::global().settings(ThrottleControl::SETTINGS_ROOT)->constantRate =
      v > 0 ? v : Throttle::UNLIMITED;
    break;

  default:
    throw internal_error("set(GValue, int) received invalid type");
  }
}

void
set(GString t, const std::string& s) {
}

}
