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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UTILS_THROTTLE_H
#define LIBTORRENT_UTILS_THROTTLE_H

#include "throttle_control.h"

namespace torrent {

class PeerConnection;

struct PeerConnectionThrottle {
  PeerConnectionThrottle(PeerConnection* p, void (PeerConnection::*f)()) : m_peer(p), m_f(f) {}

  inline void operator () () { (m_peer->*m_f)(); }

  PeerConnection* m_peer;
  void (PeerConnection::*m_f)();
};

typedef ThrottleControl<ThrottleNode<PeerConnectionThrottle> > ThrottlePeer;

extern ThrottlePeer throttleWrite;

}

#endif
