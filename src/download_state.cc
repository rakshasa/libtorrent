#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <unistd.h>
#include <inttypes.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download_state.h"
#include "peer_connection.h"
#include "peer_handshake.h"
#include "throttle_control.h"

using namespace algo;

namespace torrent {

// Temporary solution untill we get proper error handling.
extern std::list<std::string> caughtExceptions;

HashQueue hashQueue;
HashTorrent hashTorrent(&hashQueue);

DownloadState::DownloadState() :
  m_bytesUploaded(0),
  m_bytesDownloaded(0),
  m_delegator(this),
  m_settings(DownloadSettings::global()),
  m_pipeSize(&this->m_settings)
{
  m_delegator.signal_chunk_done().connect(sigc::mem_fun(*this, &DownloadState::chunk_done));
}

DownloadState::~DownloadState() {
  std::for_each(m_connections.begin(), m_connections.end(), delete_on());
}

void DownloadState::chunk_done(unsigned int index) {
  Storage::Chunk c = m_content.get_storage().get_chunk(index);

  if (!c.is_valid())
    throw internal_error("DownloadState::chunk_done(...) called with an index we couldn't retrive from storage");

  if (std::find_if(hashQueue.chunks().begin(), hashQueue.chunks().end(),

		   bool_and(eq(ref(m_hash), member(&HashQueue::Node::id)),
			    eq(value(c->get_index()), member(&HashQueue::Node::index))))
      != hashQueue.chunks().end())
    return;

  HashQueue::SignalDone& s = hashQueue.add(m_hash, c, true);

  s.connect(sigc::mem_fun(*this, &DownloadState::receive_hashdone));
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
	  (*itr)->down().c_interested() &&
	  !(*itr)->throttle().get_snub()) {
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

void DownloadState::addConnection(int fd, const PeerInfo& p) {
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

  m_signalPeerConnected.emit(Peer(c));
}

void DownloadState::removeConnection(PeerConnection* p) {
  Connections::iterator itr = std::find(m_connections.begin(), m_connections.end(), p);

  if (itr == m_connections.end())
    throw internal_error("Tried to remove peer connection from download that doesn't exist");

  m_signalPeerDisconnected.emit(Peer(*itr));

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

void DownloadState::download_stats(uint64_t& down, uint64_t& up, uint64_t& left) {
  up = m_rateUp.total();
  down = m_rateDown.total();
  left = m_content.get_size() - m_content.get_bytes_completed();

  if (left > ((uint64_t)1 << 60) ||
      (m_content.get_chunks_completed() == m_content.get_storage().get_chunkcount() && left != 0))
    throw internal_error("DownloadState::download_stats's 'left' has an invalid size"); 
}

void DownloadState::connect_peers() {
  while (!available_peers().empty() &&
	 (signed)connections().size() < settings().minPeers &&
	 countConnections() < settings().maxPeers) {

    PeerHandshake::connect(available_peers().front(), this);
    available_peers().pop_front();
  }
}

void DownloadState::receive_hashdone(std::string id, Storage::Chunk c, std::string hash) {
  if (id != m_hash)
    throw internal_error("Download received hash check comeplete signal beloning to another info hash");

  if (std::memcmp(hash.c_str(), m_content.get_hash_c(c->get_index()), 20) == 0) {

    m_content.mark_done(c->get_index());
    m_delegator.done(c->get_index());
    
    std::for_each(m_connections.begin(), m_connections.end(),
		  call_member(&PeerConnection::sendHave,
			      value(c->get_index())));

  } else {
    m_delegator.redo(c->get_index());
  }
}  

}
