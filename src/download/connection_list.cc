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

#include "protocol/peer_info.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"

#include "connection_list.h"

namespace torrent {

void
ConnectionList::clear() {
  std::for_each(begin(), end(), rak::call_delete<PeerConnectionBase>());
  Base::clear();
}

bool
ConnectionList::insert(SocketFd fd, const PeerInfo& p) {
  if (std::find_if(begin(), end(), rak::equal(p, std::mem_fun(&PeerConnectionBase::get_peer))) != end() ||
      size() >= m_maxConnections)
    return false;

  PeerConnectionBase* c = m_slotNewConnection(fd, p);

  if (c == NULL)
    throw internal_error("ConnectionList::insert(...) received a NULL pointer from m_slotNewConnection");

  Base::push_back(c);
  m_signalPeerConnected.emit(Peer(c));

  return true;
}

void
ConnectionList::erase(PeerConnectionBase* p) {
  iterator itr = std::find(begin(), end(), p);

  if (itr == end())
    throw internal_error("Tried to remove peer connection from download that doesn't exist");

  // The connection must be erased from the list before the signal is
  // emited otherwise some listeners might do stuff with the
  // assumption that the connection will remain in the list.
  Base::erase(itr);
  m_signalPeerDisconnected.emit(Peer(p));

  // Delete after the erase to ensure the connection doesn't get added
  // to the poll after PeerConnectionBase's dtor has been called.
  delete p;
}

struct _ConnectionListComp {
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return p1->get_peer().get_socket_address() < p2->get_peer().get_socket_address();
  }

  bool operator () (const SocketAddress& sa1, PeerConnectionBase* p2) const {
    return sa1 < p2->get_peer().get_socket_address();
  }

  bool operator () (PeerConnectionBase* p1, const SocketAddress& sa2) const {
    return p1->get_peer().get_socket_address() < sa2;
  }
};

void
ConnectionList::remove_connected(AddressList* l) {
  std::sort(begin(), end(), _ConnectionListComp());

  l->erase(std::set_difference(l->begin(), l->end(), begin(), end(), l->begin(), _ConnectionListComp()),
	   l->end());
}

}
