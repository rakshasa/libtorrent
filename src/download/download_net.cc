#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"

#include "download_net.h"
#include "settings.h"
#include "rate.h"
#include "peer_connection.h"
#include "throttle_control.h"

using namespace algo;

namespace torrent {

DownloadNet::~DownloadNet() {
  std::for_each(m_connections.begin(), m_connections.end(), delete_on());
}  

void
DownloadNet::set_endgame(bool b) {
  m_endgame = b;
  m_delegator.set_aggressive(b);
}

uint32_t
DownloadNet::pipe_size(const Rate& r) {
  float s = r.rate();

  if (!m_endgame)
    if (s < 50000.0f)
      return std::max(2, (int)((s + 2000.0f) / 2000.0f));
    else
      return std::min(80, (int)((s + 160000.0f) / 8000.0f));

  else
    if (s < 4000.0f)
      return 1;
    else
      return std::min(80, (int)((s + 32000.0f) / 16000.0f));
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
DownloadNet::should_request(uint32_t stall) {
  if (!m_endgame)
    return true;
  else
    return !stall || m_rateDown.rate() < m_settings->endgameRate;
}

void
DownloadNet::send_have_chunk(uint32_t index) {
  std::for_each(m_connections.begin(), m_connections.end(),
		call_member(&PeerConnection::sendHave, value(index)));
}

void
DownloadNet::add_connection(int fd, const PeerInfo& p) {
  if (std::find_if(m_connections.begin(), m_connections.end(),
		   eq(ref(p), call_member(&PeerConnection::peer))) != m_connections.end()) {
    ::close(fd);
    return;
  }

  if (count_connections() >= m_settings->maxPeers) {
    ::close(fd);
    return;
  }

  PeerConnection* c = m_slotCreateConnection(fd, p);

  if (c == NULL)
    throw internal_error("DownloadNet::add_connection(...) received a NULL pointer from m_slotCreateConnection");

  c->throttle().set_parent(&ThrottleControl::global().root());
  c->throttle().set_settings(ThrottleControl::global().settings(ThrottleControl::SETTINGS_PEER));
  c->throttle().set_socket(c);

  m_connections.push_back(c);

  PeerContainer::iterator itr = std::find(m_availablePeers.begin(), m_availablePeers.end(), p);

  if (itr != m_availablePeers.end())
    m_availablePeers.erase(itr);

  m_signalPeerConnected.emit(Peer(c));
}

void
DownloadNet::remove_connection(PeerConnection* p) {
  ConnectionList::iterator itr = std::find(m_connections.begin(), m_connections.end(), p);

  if (itr == m_connections.end())
    throw internal_error("Tried to remove peer connection from download that doesn't exist");

  m_signalPeerDisconnected.emit(Peer(*itr));

  delete *itr;
  m_connections.erase(itr);

  // TODO: Remove this when we're stable
  if (std::find(m_connections.begin(), m_connections.end(), p) != m_connections.end())
    throw internal_error("Duplicate PeerConnections in Download");

  choke_balance();
  connect_peers();
}

void
DownloadNet::add_available_peers(const PeerList& p) {
  // Make this a FIFO queue so we keep relatively fresh peers in the
  // deque. Remove old peers first when we overflow.

  for (PeerList::const_iterator itr = p.begin(); itr != p.end(); ++itr) {

    // Ignore if the peer is invalid or already known.
    if (itr->get_dns().length() == 0 || itr->get_port() == 0 ||

	std::find_if(m_connections.begin(), m_connections.end(),
		     call_member(call_member(&PeerConnection::peer), &PeerInfo::is_same_host, ref(*itr)))
	!= m_connections.end() ||

	std::find_if(m_availablePeers.begin(), m_availablePeers.end(),
		     call_member(&PeerInfo::is_same_host, ref(*itr)))
	!= m_availablePeers.end() ||

	m_slotHasHandshake(*itr))
      continue;

    m_availablePeers.push_back(*itr);
  }

  while (m_availablePeers.size() > m_settings->maxAvailable)
    m_availablePeers.pop_front();

  connect_peers();
}

int
DownloadNet::can_unchoke() {
  int s = 0;

  std::for_each(m_connections.begin(), m_connections.end(),
		if_on(bool_not(call_member(call_member(&PeerConnection::up),
					   &PeerConnection::Sub::c_choked)),
		      
		      add_ref(s, value(1))));

  return m_settings->maxUploads - s;
}

void
DownloadNet::choke_balance() {
  //  Connections::iterator itr;
  int s = can_unchoke();

  // TODO: Optimize, do a single pass with a 's' sized list of (un)chokings.
  if (s > 0) {
    m_connections.sort(gt(call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down),
			  call_member(call_member(&PeerConnection::throttle),
				      &Throttle::down)));

    // unchoke peers.
    for (ConnectionList::iterator itr = m_connections.begin();
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

    for (ConnectionList::iterator itr = m_connections.begin();
	 itr != m_connections.end() && s != 0; ++itr) {
      
      if (!(*itr)->up().c_choked()) {
	(*itr)->choke(true);
	++s;
      }
    }
  }    
}

void
DownloadNet::connect_peers() {
  while (!m_availablePeers.empty() &&
	 (signed)m_connections.size() < m_settings->minPeers &&
	 count_connections() < m_settings->maxPeers) {

    m_slotStartHandshake(m_availablePeers.front());
    m_availablePeers.pop_front();
  }
}

int
DownloadNet::count_connections() const {
  return m_connections.size() + m_slotCountHandshakes();
}
}
