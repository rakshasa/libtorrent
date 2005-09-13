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
ConnectionList::insert(DownloadMain* d, const PeerInfo& p, const SocketFd& fd) {
  if (std::find_if(begin(), end(), rak::equal(p, std::mem_fun(&PeerConnectionBase::get_peer))) != end() ||
      size() >= m_maxSize)
    return false;

  PeerConnectionBase* c = m_slotNewConnection();

  if (c == NULL)
    throw internal_error("ConnectionList::insert(...) received a NULL pointer from m_slotNewConnection");

  c->initialize(d, p, fd);

  Base::push_back(c);
  m_signalConnected.emit(Peer(c));

  return true;
}

ConnectionList::iterator
ConnectionList::erase(iterator pos) {
  if (pos < begin() || pos >= end())
    throw internal_error("ConnectionList::erase(...) iterator out or range.");

  value_type v = *pos;

  pos = Base::erase(pos);
  m_signalDisconnected.emit(Peer(v));

  // Delete after the erase to ensure the connection doesn't get added
  // to the poll after PeerConnectionBase's dtor has been called.
  delete v;

  return pos;
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
  m_signalDisconnected.emit(Peer(p));

  // Delete after the erase to ensure the connection doesn't get added
  // to the poll after PeerConnectionBase's dtor has been called.
  delete p;
}

void
ConnectionList::erase_remaining(iterator pos) {
  // Need to do it one connection at the time to ensure that when the
  // signal is emited everything is in a valid state.
  while (pos != end()) {
    value_type v = Base::back();

    Base::pop_back();

    m_signalDisconnected.emit(Peer(v));
    delete v;
  }
}

void
ConnectionList::erase_seeders() {
  erase_remaining(std::partition(begin(), end(), std::not1(std::mem_fun(&PeerConnectionBase::is_seeder))));
}

struct _ConnectionListCompLess {
  bool operator () (const PeerConnectionBase* p1, const PeerConnectionBase* p2) const {
    return p1->get_peer().get_socket_address() < p2->get_peer().get_socket_address();
  }

  bool operator () (const SocketAddress& sa1, const PeerConnectionBase* p2) const {
    return sa1 < p2->get_peer().get_socket_address();
  }

  bool operator () (const PeerConnectionBase* p1, const SocketAddress& sa2) const {
    return p1->get_peer().get_socket_address() < sa2;
  }
};

ConnectionList::iterator
ConnectionList::find(const SocketAddress& sa) {
  return std::find_if(begin(), end(),
		      rak::equal(sa, rak::on(std::mem_fun(&PeerConnectionBase::get_peer),
					     std::mem_fun_ref<const torrent::SocketAddress&>(&PeerInfo::get_socket_address))));
}

void
ConnectionList::set_difference(AddressList* l) {
  std::sort(begin(), end(), _ConnectionListCompLess());

  l->erase(std::set_difference(l->begin(), l->end(), begin(), end(), l->begin(), _ConnectionListCompLess()),
	   l->end());
}

void
ConnectionList::send_have_chunk(uint32_t index) {
  std::for_each(begin(), end(), std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_have_chunk), index));
}

}
