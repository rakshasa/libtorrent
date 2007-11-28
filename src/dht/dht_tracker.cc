// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include "torrent/object.h"

#include "dht_tracker.h"

namespace torrent {

void
DhtTracker::add_peer(uint32_t addr, uint16_t port) {
  if (port == 0)
    return;

  SocketAddressCompact compact(addr, port);

  unsigned int oldest = 0;
  uint32_t minSeen = ~uint32_t();

  // Check if peer exists. If not, find oldest peer.
  for (unsigned int i = 0; i < size(); i++) {
    if (m_peers[i].addr == compact.addr) {
      m_peers[i].port = compact.port;
      m_lastSeen[i] = cachedTime.seconds();
      return;

    } else if (m_lastSeen[i] < minSeen) {
      minSeen = m_lastSeen[i];
      oldest = i;
    }
  }

  // If peer doesn't exist, append to list if the table is not full.
  if (size() < max_size) {
    m_peers.push_back(compact);
    m_lastSeen.push_back(cachedTime.seconds());

  // Peer doesn't exist and table is full: replace oldest peer.
  } else {
    m_peers[oldest] = compact;
    m_lastSeen[oldest] = cachedTime.seconds();
  }
}

// Return compact info (6 bytes) for up to 30 peers, returning different
// peers for each call if there are more.
std::string
DhtTracker::get_peers(unsigned int maxPeers) {
  PeerList::iterator first = m_peers.begin();
  PeerList::iterator last = m_peers.end();

  // If we have more than max_peers, randomly return block of peers.
  // The peers in overlapping blocks get picked twice as often, but
  // that's better than returning fewer peers.
  if (m_peers.size() > maxPeers) {
    unsigned int blocks = (m_peers.size() + maxPeers - 1) / maxPeers;

    first += (random() % blocks) * (m_peers.size() - maxPeers) / (blocks - 1);
    last = first + maxPeers;
  }

  return std::string(first->c_str(), last->c_str());
}

// Remove old announces.
void
DhtTracker::prune(uint32_t maxAge) {
  uint32_t minSeen = cachedTime.seconds() - maxAge;

  for (unsigned int i = 0; i < m_lastSeen.size(); i++)
    if (m_lastSeen[i] < minSeen) m_peers[i].port = 0;

  m_peers.erase(std::remove_if(m_peers.begin(), m_peers.end(), rak::on(rak::mem_ref(&SocketAddressCompact::port), std::bind2nd(std::equal_to<uint16_t>(), 0))), m_peers.end());
  m_lastSeen.erase(std::remove_if(m_lastSeen.begin(), m_lastSeen.end(), std::bind2nd(std::less<uint32_t>(), minSeen)), m_lastSeen.end());

  if (m_peers.size() != m_lastSeen.size())
    throw internal_error("DhtTracker::prune did inconsistent peer pruning.");
}

}
