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
  std::for_each(m_connectionList.begin(), m_connectionList.end(),
		std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_have_chunk), index));
}

void
DownloadNet::connect_peers() {
  while (!m_availableList.empty() &&
	 m_connectionList.size() < m_connectionList.get_min_size() &&
	 count_connections() < m_connectionList.get_max_size()) // Might not need this...
    m_slotStartHandshake(m_availableList.pop_random());
}

uint32_t
DownloadNet::count_connections() const {
  return m_connectionList.size() + m_slotCountHandshakes();
}

void
DownloadNet::receive_remove_available(Peer p) {
  AvailableList::iterator itr = std::find(m_availableList.begin(), m_availableList.end(),
					  p.get_ptr()->get_peer().get_socket_address());

  if (itr != m_availableList.end())
    m_availableList.erase(itr);
}

}
