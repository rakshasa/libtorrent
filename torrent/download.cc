#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "download.h"
#include "files_check.h"
#include "general.h"
#include "listen.h"
#include "peer_handshake.h"
#include "peer_connection.h"
#include "settings.h"
#include "tracker_query.h"

#include <sstream>
#include <unistd.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

Download::Downloads Download::m_downloads;

Download::Download(const bencode& b) :
  m_bytesUploaded(0),
  m_bytesDownloaded(0),
  m_tracker(NULL),
  m_checked(false),
  m_settings(DownloadSettings::global())
{
  try {

  m_me = Peer(generateId(), "", Listen::port());
  m_name = b["info"]["name"].asString();

  m_files.set(b["info"]);
  m_files.openAll();

  m_tracker = new TrackerQuery(this);

  m_tracker->set(b["announce"].asString(),
		 calcHash(b["info"]),
		 m_me);

  FilesCheck::check(&m_files, this, HASH_COMPLETED);

  m_delegator = Delegator(&m_files.bitfield(),
			  m_files.chunkSize(),
			  m_files.chunkSize(m_files.chunkCount() - 1));

  } catch (const bencode_error& e) {

    m_files.closeAll();
    delete m_tracker;

    throw local_error("Bad torrent file \"" + std::string(e.what()) + "\"");
  } catch (const local_error& e) {

    m_files.closeAll();
    delete m_tracker;

    throw e;
  }
}

Download::~Download() {
  std::for_each(m_connections.begin(), m_connections.end(), delete_on());

  m_files.closeAll();
  delete m_tracker;

  m_downloads.erase(std::find(m_downloads.begin(), m_downloads.end(), this));
}

void Download::start() {
  if (m_tracker->state() != TrackerQuery::STOPPED)
    return;

  m_tracker->state(TrackerQuery::STARTED, m_checked);

  insertService(Timer::current() + m_settings.chokeCycle * 2, CHOKE_CYCLE);
}  


void Download::stop() {
  if (m_tracker->state() == TrackerQuery::STOPPED)
    return;

  m_tracker->state(TrackerQuery::STOPPED);

  removeService(CHOKE_CYCLE);

  // TODO, handle stopping of download correctly.
}

void Download::service(int type) {
  int s;
  Connections::iterator cItr1;
  Connections::reverse_iterator cItr2;

  switch (type) {
  case HASH_COMPLETED:
    m_checked = true;
    m_files.resizeAll();

    if (m_tracker->state() == TrackerQuery::STARTED)
      m_tracker->state(TrackerQuery::STARTED);

    // TODO: Remove this
    if (m_files.chunkCompleted() == m_files.chunkCount() &&
	!m_files.bitfield().allSet())
      throw internal_error("BitField.allSet() bork");

    return;
    
  case CHOKE_CYCLE:
    insertService(Timer::cache() + m_settings.chokeCycle, CHOKE_CYCLE);

    // Clean up the download rate in case the client doesn't read
    // it regulary.
    m_rateUp.rate();
    m_rateDown.rate();

    s = canUnchoke();

    if (s > 0)
      // If we haven't filled up out chokes then we shouldn't don't cycle.
      return;

    m_connections.sort(lt(call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate),
			  call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate)));

    // First unchoked connection. TODO: Add bool_and timer last choked
    cItr1 = std::find_if(m_connections.begin(), m_connections.end(),
			 bool_not(call_member(call_member(&PeerConnection::up),
					      &PeerConnection::Sub::c_choked)));

    if (cItr1 == m_connections.end())
      return;

    // TODO: Sort so we help newbies get their first chunk if we are far
    // or unchoke an untried peers then according to average? Unchoke peers
    // that we are interested in?

    // TODO: Include the Snub factor, allow untried snubless peers to download too.

    // TODO: Prefer peers we are interested in, unless we are being helpfull to newcomers.

    cItr2 = std::find_if(m_connections.rbegin(), m_connections.rend(),
			 call_member(call_member(&PeerConnection::up),
				     &PeerConnection::Sub::c_choked));

    if (cItr2 == m_connections.rend())
      return;

    (*cItr1)->choke(true);
    (*cItr2)->choke(false);

    return;

  default:
    throw internal_error("Download::service called with bad argument");
  };
}

bool Download::isStopped() {
  return m_tracker->state() == TrackerQuery::STOPPED &&
    !m_tracker->busy();
}

void Download::addConnection(int fd, const Peer& p) {
  if (std::find_if(m_connections.begin(), m_connections.end(),
		   eq(ref(p), call_member(&PeerConnection::peer))) !=
      m_connections.end()) {
    // TODO: Close current, not this one. (Only if current is dead?)
    ::close(fd);

    return;
  }

  if (countConnections() >= m_settings.maxPeers) {
    ::close(fd);
    
    return;
  }

  PeerConnection* c = new PeerConnection();

  c->set(fd, p, this);
  m_connections.push_back(c);

  Peers::iterator itr = std::find(m_availablePeers.begin(), m_availablePeers.end(), p);

  if (itr != m_availablePeers.end())
    m_availablePeers.erase(itr);
}

void Download::removeConnection(PeerConnection* p) {
  Connections::iterator itr = std::find(m_connections.begin(), m_connections.end(), p);

  if (itr == m_connections.end())
    throw internal_error("Tried to remove peer connection from download that doesn't exist");

  delete *itr;
  m_connections.erase(itr);

  if (std::find(m_connections.begin(), m_connections.end(), p) != m_connections.end())
    throw internal_error("Duplicate PeerConnections in Download");

  chokeBalance();
  connectPeers();
}

void Download::chunkDone(Chunk& c) {
  if (m_files.doneChunk(c)) {
    m_delegator.done(c.index());
    
    std::for_each(m_connections.begin(), m_connections.end(),
		  call_member(&PeerConnection::sendHave,
			      value(c.index())));

  } else {
    m_delegator.redo(c.index());

    // TODO: Hash fail counter
  }
}

Download* Download::getDownload(const std::string& hash) {
  Downloads::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					 eq(ref(hash), call_member(call_member(&Download::tracker),
								   &TrackerQuery::hash)));
 
  return itr != m_downloads.end() ? *itr : NULL;
}

void Download::addPeer(const Peer& p) {
  if (p == m_me ||

      std::find_if(m_connections.begin(), m_connections.end(),
		   eq(call_member(&PeerConnection::peer),
		      ref(p)))
      != m_connections.end() ||

      std::find_if(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		   eq(call_member(&PeerHandshake::peer),
		      ref(p)))
      != PeerHandshake::handshakes().end() ||

      std::find(m_availablePeers.begin(), m_availablePeers.end(), p)
      != m_availablePeers.end())
    // We already know this peer
    return;

  // Push to back since we want to connect to old peers since they are more
  // likely to have more of the file. This also makes sure we don't end up with
  // lots of old dead peers in the stack.
  m_availablePeers.push_back(p);
}

void Download::connectPeers() {
  while (!m_availablePeers.empty() &&
	 (signed)m_connections.size() < m_settings.minPeers &&
	 countConnections() < m_settings.maxPeers) {

    PeerHandshake::connect(m_availablePeers.front(), this);
    m_availablePeers.pop_front();
  }
}

void Download::chokeBalance() {
  int s = canUnchoke();

  if (s > 0) {
    m_connections.sort(gt(call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate),
			  call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate)));

    // unchoke peers.
    for (Connections::iterator itr = m_connections.begin();
	 itr != m_connections.end() && s != 0; ++itr) {
      
      if ((*itr)->up().c_choked() &&
	  (*itr)->down().c_interested()) {
	(*itr)->choke(false);
	--s;
      }
    }

  } else if (s < 0) {
    // Sort so we choke slow uploaders first.
    // TODO: Should we sort by unchoked too?
    m_connections.sort(lt(call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate),
			  call_member(call_member(&PeerConnection::down),
				      &PeerConnection::Sub::c_rate)));

    for (Connections::iterator itr = m_connections.begin();
	 itr != m_connections.end() && s != 0; ++itr) {
      
      if (!(*itr)->up().c_choked()) {
	(*itr)->choke(true);
	++s;
      }
    }
  }    
}

int Download::countConnections() const {
  int s = m_connections.size();

  std::for_each(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		if_on(eq(call_member(&PeerHandshake::download),
			 value(this)),

		      add_ref(s, value(1))));

  return s;
}

int Download::canUnchoke() {
  int s = 0;

  std::for_each(m_connections.begin(), m_connections.end(),
		if_on(bool_not(call_member(call_member(&PeerConnection::up),
					   &PeerConnection::Sub::c_choked)),
		      
		      add_ref(s, value(1))));

  return m_settings.maxUploads - s;
}

}
