#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <inttypes.h>
#include <algo/algo.h>

#include "exceptions.h"
#include "download_state.h"
#include "peer_connection.h"
#include "peer_handshake.h"
#include "throttle_control.h"

using namespace algo;

namespace torrent {

// Temporary solution untill we get proper error handling.
extern std::list<std::string> caughtExceptions;

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

void DownloadState::chunkDone(Storage::Chunk& c) {
  if (m_files.doneChunk(c)) {
    m_delegator.done(c->get_index());
    
    std::for_each(m_connections.begin(), m_connections.end(),
		  call_member(&PeerConnection::sendHave,
			      value(c->get_index())));

  } else {
    m_delegator.redo(c->get_index());
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
  //  Connections::iterator itr;
  int s = canUnchoke();

  // TODO: Optimize, do a single pass with a 's' sized list of (un)chokings.
  if (s > 0) {
    m_connections.sort(gt(call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down),
			  call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down)));

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
    m_connections.sort(lt(call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down),
			  call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down)));

    for (Connections::iterator itr = m_connections.begin();
	 itr != m_connections.end() && s != 0; ++itr) {
      
      if (!(*itr)->up().c_choked()) {
	(*itr)->choke(true);
	++s;
      }
    }
  }    
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

  c->throttle().set_parent(&ThrottleControl::global().root());
  c->throttle().set_settings(ThrottleControl::global().settings(ThrottleControl::SETTINGS_PEER));
  c->throttle().set_socket(c);

  m_connections.push_back(c);

  Peers::iterator itr = std::find(m_availablePeers.begin(), m_availablePeers.end(), p);

  if (itr != m_availablePeers.end())
    m_availablePeers.erase(itr);
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
  connect_peers();
}

int DownloadState::countConnections() const {
  int s = m_connections.size();

  std::for_each(PeerHandshake::handshakes().begin(), PeerHandshake::handshakes().end(),
		if_on(eq(call_member(&PeerHandshake::download),
			 value(this)),

		      add_ref(s, value(1))));

  return s;
}

void DownloadState::download_stats(uint64_t& up, uint64_t& down, uint64_t& left) {
  up = m_rateUp.total();
  down = m_rateDown.total();
  left = m_files.storage().get_size() - m_files.doneSize();
}

void DownloadState::connect_peers() {
  while (!available_peers().empty() &&
	 (signed)connections().size() < settings().minPeers &&
	 countConnections() < settings().maxPeers) {

//     std::stringstream s;
    
//     s << "Connecting to " << available_peers().front().dns() << ':' << available_peers().front().port();

//     caughtExceptions.push_front(s.str());

    PeerHandshake::connect(available_peers().front(), this);
    available_peers().pop_front();
  }
}

}
