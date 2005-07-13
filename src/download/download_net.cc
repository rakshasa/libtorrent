// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <algorithm>
#include <rak/functional.h>

#include "torrent/exceptions.h"

#include "download_net.h"
#include "settings.h"
#include "peer_connection.h"

namespace torrent {

DownloadNet::DownloadNet() :
  m_settings(NULL),
  m_endgame(false),

  m_writeRate(60),
  m_readRate(60) {
}

DownloadNet::~DownloadNet() {
  std::for_each(m_connections.begin(), m_connections.end(), rak::call_delete<PeerConnectionBase>());
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
      return std::min(200, (int)((s + 160000.0f) / 4000.0f));

  else
    if (s < 4000.0f)
      return 1;
    else
      return std::min(80, (int)((s + 32000.0f) / 8000.0f));
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
DownloadNet::should_request(uint32_t stall) {
  if (!m_endgame)
    return true;
  else
    // We check if the peer is stalled, if it is not then we should
    // request. If the peer is stalled then we only request if the
    // download rate is below a certain value.
    return !stall || m_readRate.rate() < m_settings->endgameRate;
}

void
DownloadNet::send_have_chunk(uint32_t index) {
  std::for_each(m_connections.begin(), m_connections.end(),
		std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_have_chunk), index));
}

bool
DownloadNet::add_connection(SocketFd fd, const PeerInfo& p) {
  if (std::find_if(m_connections.begin(), m_connections.end(),
		   rak::equal(p, std::mem_fun(&PeerConnectionBase::get_peer)))
      != m_connections.end()) {
    return false;
  }

  if (count_connections() >= m_settings->maxPeers) {
    return false;
  }

  PeerConnectionBase* c = m_slotCreateConnection(fd, p);

  if (c == NULL)
    throw internal_error("DownloadNet::add_connection(...) received a NULL pointer from m_slotCreateConnection");

  m_connections.push_back(c);

  PeerContainer::iterator itr = std::find(m_availablePeers.begin(), m_availablePeers.end(), p);

  if (itr != m_availablePeers.end())
    m_availablePeers.erase(itr);

  m_signalPeerConnected.emit(Peer(c));

  return true;
}

void
DownloadNet::remove_connection(PeerConnectionBase* p) {
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
DownloadNet::add_available_peers(const PeerList* p) {
  // Make this a FIFO queue so we keep relatively fresh peers in the
  // deque. Remove old peers first when we overflow.

  for (PeerList::const_iterator itr = p->begin(); itr != p->end(); ++itr) {

    // Ignore if the peer is invalid or already known.
    if (itr->get_dns().length() == 0 || itr->get_port() == 0 ||

	std::find_if(m_connections.begin(), m_connections.end(),
		     rak::on(std::mem_fun(&PeerConnectionBase::get_peer),
			     std::bind2nd(std::mem_fun_ref(&PeerInfo::is_same_host_value), *itr)))
	!= m_connections.end() ||

	std::find_if(m_availablePeers.begin(), m_availablePeers.end(),
		     std::bind2nd(std::mem_fun_ref(&PeerInfo::is_same_host_value), *itr))
	!= m_availablePeers.end() ||

	m_slotHasHandshake(*itr))
      continue;

    m_availablePeers.push_back(*itr);
  }

  while (m_availablePeers.size() > m_settings->maxAvailable)
    m_availablePeers.pop_front();

  connect_peers();
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
