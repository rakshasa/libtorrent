#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <algo/algo.h>
#include <iostream>
#include <cassert>

#include <sigc++/signal.h>

#include "exceptions.h"
#include "download.h"
#include "general.h"
#include "net/listen.h"
#include "peer_handshake.h"
#include "peer_connection.h"
#include "throttle_control.h"
#include "tracker/tracker_control.h"
#include "settings.h"
#include "timer.h"
#include "url/curl_stack.h"
#include "torrent.h"

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

void initialize(int beginPort, int endPort) {
  if (listen == NULL) {
    listen = new Listen;

    listen->signal_incoming().connect(sigc::ptr_fun3(&PeerHandshake::connect));
  }

  srandom(Timer::current().usec());

  if (!listen->open(beginPort, endPort))
    throw local_error("Could not open port for listening");

  ThrottleControl::global().insert_service(Timer::current(), 0);

  CurlStack::global_init();
}

void shutdown() {
  listen->close();

  std::for_each(Download::downloads().begin(), Download::downloads().end(),
		call_member(&Download::stop));
}

// Clean up, close stuff. Calling, and waiting, for shutdown is
// not required, but highly recommended.
void cleanup() {
  // Close again if shutdown wasn't called.
  listen->close();
 
  ThrottleControl::global().remove_service();

  for_each<true>(Download::downloads().begin(), Download::downloads().end(),
		 delete_on());

  for_each<true>(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		 delete_on());

  CurlStack::global_cleanup();
}

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
int mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  int maxFd = 0;

  if (curlStack.is_busy())
    curlStack.fdset(readSet, writeSet, exceptSet, maxFd);

  maxFd = std::max(maxFd, std::for_each(SocketBase::readSockets().begin(),
					SocketBase::readSockets().end(),
					add_socket(readSet)).fd);

  maxFd = std::max(maxFd, std::for_each(SocketBase::writeSockets().begin(),
					SocketBase::writeSockets().end(),
					add_socket(writeSet)).fd);

  maxFd = std::max(maxFd, std::for_each(SocketBase::exceptSockets().begin(),
					SocketBase::exceptSockets().end(),
					add_socket(exceptSet)).fd);

  return maxFd;
}    

// Do work on the file descriptors.
void work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  // Update the cached time.
  Timer::update();

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL)
    throw internal_error("Torrent::work received a NULL pointer to a fs_set");

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

  if (curlStack.is_busy())
    curlStack.perform();

  Service::perform_service();
}

// It will be parsed through a stream anyway, so no need to supply
// a function that handles char arrays. Just use std::stringstream.
DList::const_iterator create(std::istream& s) {
  // TODO: Should we clear failed bits?
  s.clear();

  bencode b;

  s >> b;

  if (s.fail())
    // Make it configurable whetever we throw or return .end()?
    throw local_error("Could not parse bencoded torrent");
  
  Download* download = new Download(b);

  return Download::downloads().insert(Download::downloads().end(), download);
}

// List container with all the current downloads.
const DList& downloads() {
  return Download::downloads();
}

// List of all connected peers in 'd'.
const PList& peers(DList::const_iterator d) {
  return (*d)->state().connections();
}

// Call this once you've (preferably though not required) stopped the
// download. Do not use the iterator after calling this function,
// pre-increment it or something.
void remove(DList::const_iterator d) {
  // Removing downloads happen seldomly and this provides abit more
  // security. (Besides, i really don't want to cast it to a non-const
  // iterator since those are two different structs)
  DList::iterator itr = std::find_if(Download::downloads().begin(), Download::downloads().end(),
				     eq(call_member(call_member(&Download::state), &DownloadState::hash),
					ref((*d)->state().hash())));

  if (itr == Download::downloads().end())
    throw internal_error("Application tried to remove a non-existant download");

  delete *itr;

  //Download::downloads().erase(itr);
}

bool start(DList::const_iterator d) {
  (*d)->start();

  return true;
}

bool stop(DList::const_iterator d) {
  (*d)->stop();

  return true;
}

// Throws a local_error of some sort.
int64_t get(GValue t) {
  switch (t) {
  case LISTEN_PORT:
    return listen->get_port();

  case HANDSHAKES_TOTAL:
    return PeerHandshake::handshakes().size();

  case SHUTDOWN_DONE:
    return std::find_if(Download::downloads().begin(), Download::downloads().end(),
			bool_not(call_member(&Download::isStopped)))
      == Download::downloads().end();

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

  case HTTP_GETS:
    return curlStack.get_size();

  default:
    throw internal_error("get(GValue) received invalid type");
  }
}

std::string get(GString t) {
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

int64_t get(DList::const_iterator d, DValue t) {
  Timer::update();
  uint64_t a;

  switch (t) {
  case BYTES_DOWNLOADED:
    return (*d)->state().bytesDownloaded();

  case BYTES_UPLOADED:
    return (*d)->state().bytesUploaded();

  case BYTES_TOTAL:
    return (*d)->state().content().get_storage().get_size();

  case BYTES_DONE:
    a = 0;

    std::for_each((*d)->state().delegator().chunks().begin(), (*d)->state().delegator().chunks().end(),
		  for_each_on(member(&Delegator::Chunk::m_pieces),
			      if_on(eq(value(Delegator::FINISHED),
				       member(&Delegator::PieceInfo::m_state)),
				    
				    add_ref(a, call_member(member(&Delegator::PieceInfo::m_piece),
							   &Piece::length)))));

    return a + (*d)->state().content().get_completed() * (*d)->state().content().get_storage().get_chunksize();

  case CHUNKS_DONE:
    return (*d)->state().content().get_completed();

  case CHUNKS_SIZE:
    return (*d)->state().content().get_storage().get_chunksize();

  case CHUNKS_TOTAL:
    return (*d)->state().content().get_storage().get_chunkcount();

  case CHOKE_CYCLE:
    return (*d)->state().settings().chokeCycle;

  case RATE_UP:
    return (*d)->state().rateUp().rate_quick();

  case RATE_DOWN:
    return (*d)->state().rateDown().rate_quick();

  case PEERS_MIN:
    return (*d)->state().settings().minPeers;

  case PEERS_MAX:
    return (*d)->state().settings().maxPeers;

  case PEERS_CONNECTED:
    return (*d)->state().connections().size();

  case PEERS_NOT_CONNECTED:
    return (*d)->state().available_peers().size();

  case TRACKER_CONNECTING:
    return (*d)->tracker().is_busy();

  case TRACKER_TIMEOUT:
    return (*d)->tracker().get_next().usec();

  case UPLOADS_MAX:
    return (*d)->state().settings().maxUploads;

  case IS_STOPPED:
    return (*d)->isStopped();

  case ENTRY_COUNT:
    return (*d)->state().content().get_files().size();

  default:
    throw internal_error("get(itr, DValue) received invalid type");
  }
}    

std::string get(DList::const_iterator d, DString t) {
  std::string s;

  switch (t) {
  case BITFIELD_LOCAL:
    return std::string((*d)->state().content().get_bitfield().data(), (*d)->state().content().get_bitfield().sizeBytes());

  case BITFIELD_SEEN:
    std::for_each((*d)->state().bfCounter().field().begin(),
		  (*d)->state().bfCounter().field().end(),
		  call_member(ref(s), &std::string::push_back,

			      if_on<char>(lt(back_as_ref(), value(256)),
					  back_as_ref(),
					  value(255))));

    return s;

  case INFO_NAME:
    return (*d)->name();

  case INFO_HASH:
    return (*d)->state().hash();

  case TRACKER_MSG:
    return ""; //(*d)->tracker().msg();

  default:
    throw internal_error("get(itr, DString) received invalid type");
  }
}    

int64_t get(PList::const_iterator p, PValue t) {
  Timer::update();

  switch (t) {
  case PEER_LOCAL_CHOKED:
    return (*p)->up().c_choked();

  case PEER_LOCAL_INTERESTED:
    return (*p)->up().c_interested();

  case PEER_REMOTE_CHOKED:
    return (*p)->down().c_choked();

  case PEER_REMOTE_INTERESTED:
    return (*p)->down().c_interested();

  case PEER_CHOKE_DELAYED:
    return (*p)->chokeDelayed();

  case PEER_RATE_DOWN:
    return (*p)->throttle().down().rate_quick();

  case PEER_RATE_UP:
    return (*p)->throttle().up().rate_quick();

  case PEER_PORT:
    return (*p)->peer().port();

  case PEER_SNUB:
    return (*p)->throttle().get_snub();

  default:
    throw internal_error("get(itr, PValue) received invalid type");
  }
}    

std::string get(PList::const_iterator p, PString t) {
  std::string s;

  switch (t) {
  case PEER_ID:
    return (*p)->peer().id();

  case PEER_DNS:
    return (*p)->peer().dns();

  case PEER_BITFIELD:
    return std::string((*p)->bitfield().data(), (*p)->bitfield().sizeBytes());

  case PEER_INCOMING:
    std::for_each((*p)->down().c_list().begin(), (*p)->down().c_list().end(),
		  add_ref(s, call_ctor<std::string>(convert_ptr<char>(call_member(&Piece::index)),
						    value(4))));

    return s;

  case PEER_OUTGOING:
    std::for_each((*p)->up().c_list().begin(), (*p)->up().c_list().end(),
		  add_ref(s, call_ctor<std::string>(convert_ptr<char>(call_member(&Piece::index)),
						    value(4))));

    return s;

  default:
    throw internal_error("get(itr, PString) received invalid type");
  }
}

void set(GValue t, int64_t v) {
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

void set(GString t, const std::string& s) {
}

void set(DList::const_iterator d, DValue t, int64_t v) {
  Timer timer;

  switch (t) {
  case PEERS_MIN:
    if (v > 0 && v < 1000) {
      (*d)->state().settings().minPeers = v;
      (*d)->state().connect_peers();
    }

    break;

  case PEERS_MAX:
    if (v > 0 && v < 1000) {
      (*d)->state().settings().maxPeers = v;
      // TODO: Do disconnects here if nessesary
    }

    break;

  case UPLOADS_MAX:
    if (v > 0 && v < 1000) {
      (*d)->state().settings().maxUploads = v;
      (*d)->state().chokeBalance();
    }

    break;

  case CHOKE_CYCLE:
    if (v < 10 * 1000000 || v >= 3600 * 1000000)
      break;

    if ((*d)->in_service(Download::CHOKE_CYCLE)) {
      timer = (*d)->when_service(Download::CHOKE_CYCLE);

      (*d)->remove_service(Download::CHOKE_CYCLE);
      (*d)->insert_service(timer - (*d)->state().settings().chokeCycle + v,
			  Download::CHOKE_CYCLE);
    }

    (*d)->state().settings().chokeCycle = v;

    break;

  case TRACKER_TIMEOUT:
    (*d)->tracker().set_next(v);

    break;

  default:
    throw internal_error("set(GValue, int) received invalid type");
  }
}

void set(DList::const_iterator d, DString t, const std::string& s) {
}

void set(PList::const_iterator p, PValue t, int64_t v) {
  switch (t) {
  case PEER_SNUB:
    (*p)->throttle().set_snub(v);
    (*p)->choke(true);
    break;

  default:
    throw internal_error("set(PItr, PValue, int) received invalid type");
  }
}

SignalDownloadDone& signalDownloadDone(DList::const_iterator itr) {
  return (*itr)->state().content().signal_download_done();
}

Entry get_entry(DItr itr, unsigned int index) {
  if (index >= (*itr)->state().content().get_files().size())
    throw internal_error("Client tried to access file with index out of bounds");

  return Entry(&(*itr)->state().content().get_files()[index]);
}

void update_priorities(DItr itr) {
  Priority& p = (*itr)->state().delegator().select().get_priority();

  p.clear();

  uint64_t pos = 0;
  unsigned int cs = (*itr)->state().content().get_storage().get_chunksize();

  for (Content::FileList::const_iterator i = (*itr)->state().content().get_files().begin();
       i != (*itr)->state().content().get_files().end(); ++i) {
    
    unsigned int s = pos / cs;
    unsigned int e = i->get_size() ? (pos + i->get_size() + cs - 1) / cs : s;

    if (s != e)
      p.add((Priority::Type)i->get_priority(), s, e);

    pos += i->get_size();
  }

//   if (p.get_list(Priority::NORMAL).empty())
//     throw internal_error("Empty priorities list after update");
}

}
