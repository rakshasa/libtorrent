#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "download_state.h"
#include "peer_connection.h"
#include "peer_handshake.h"
#include <algo/algo.h>

using namespace algo;

namespace torrent {

DownloadState::DownloadState() :
  m_bytesUploaded(0),
  m_bytesDownloaded(0),
  m_delegator(this),
  m_settings(DownloadSettings::global())
{
}

DownloadState::~DownloadState() {
  std::for_each(m_connections.begin(), m_connections.end(), delete_on());

  m_files.closeAll();
}

void DownloadState::removeConnection(PeerConnection* p) {
  Connections::iterator itr = std::find(m_connections.begin(), m_connections.end(), p);

  if (itr == m_connections.end())
    throw internal_error("Tried to remove peer connection from download that doesn't exist");

  delete *itr;
  m_connections.erase(itr);

  // TODO: Remove this when we're stable
  if (std::find(m_connections.begin(), m_connections.end(), p) != m_connections.end())
    throw internal_error("Duplicate PeerConnections in Download");

  chokeBalance();
  connectPeers();
}

void DownloadState::chunkDone(Chunk& c) {
  if (m_files.doneChunk(c)) {
    m_delegator.done(c.index());
    
    std::for_each(m_connections.begin(), m_connections.end(),
		  call_member(&PeerConnection::sendHave,
			      value(c.index())));

  } else {
    m_delegator.redo(c.index());

    // TODO: Hash fail logic needed. Redo piece by piece
  }
}

int DownloadState::canUnchoke() {
  int s = 0;

  std::for_each(m_connections.begin(), m_connections.end(),
		if_on(bool_not(call_member(call_member(&PeerConnection::up),
					   &PeerConnection::Sub::c_choked)),
		      
		      add_ref(s, value(1))));

  return m_settings.maxUploads - s;
}

void DownloadState::chokeBalance() {
  int s = canUnchoke();

  // TODO: Optimize, do a single pass with a 's' sized list of (un)chokings.
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

void DownloadState::addPeer(const Peer& p) {
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

void DownloadState::addConnection(int fd, const Peer& p) {
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

void DownloadState::connectPeers() {
  while (!m_availablePeers.empty() &&
	 (signed)m_connections.size() < m_settings.minPeers &&
	 countConnections() < m_settings.maxPeers) {

    PeerHandshake::connect(m_availablePeers.front(), this);
    m_availablePeers.pop_front();
  }
}

int DownloadState::countConnections() const {
  int s = m_connections.size();

  std::for_each(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		if_on(eq(call_member(&PeerHandshake::download),
			 value(this)),

		      add_ref(s, value(1))));

  return s;
}

}
