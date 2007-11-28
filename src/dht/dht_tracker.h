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

#ifndef LIBTORRENT_DHT_TRACKER_H
#define LIBTORRENT_DHT_TRACKER_H

#include "globals.h"

#include <vector>
#include <rak/socket_address.h>

#include "download/download_info.h"  // for SocketAddressCompact

namespace torrent {

// Container for peers tracked in a torrent.

class DhtTracker {
public:
  // Maximum number of peers we return for a GET_PEERS query (default value only). 
  // Needs to be small enough so that a packet with a payload of num_peers*6 bytes 
  // does not need fragmentation. Value chosen so that the size is approximately
  // equal to a FIND_NODE reply (8*26 bytes).
  static const unsigned int max_peers = 32;

  // Maximum number of peers we keep track of. For torrents with more peers,
  // we replace the oldest peer with each new announce to avoid excessively
  // large peer tables for very active torrents.
  static const unsigned int max_size = 128;

  bool                empty() const                { return m_peers.empty(); }
  size_t              size() const                 { return m_peers.size(); }

  void                add_peer(uint32_t addr, uint16_t port);
  std::string         get_peers(unsigned int maxPeers = max_peers);

  // Remove old announces from the tracker that have not reannounced for
  // more than the given number of seconds.
  void                prune(uint32_t maxAge);

private:
  typedef std::vector<SocketAddressCompact> PeerList;

  PeerList               m_peers;
  std::vector<uint32_t>  m_lastSeen;
};

}

#endif
