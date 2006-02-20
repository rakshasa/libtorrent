// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "net/manager.h"
#include "net/socket_address.h"
#include "torrent/exceptions.h"

#include "peer_info.h"
#include "handshake_manager.h"
#include "handshake_incoming.h"
#include "handshake_outgoing.h"

namespace torrent {

HandshakeManager::size_type
HandshakeManager::size_info(TrackerInfo* info) const {
  return std::count_if(Base::begin(), Base::end(), rak::equal(info, std::mem_fun(&Handshake::info)));
}

void
HandshakeManager::clear() {
  std::for_each(Base::begin(), Base::end(), std::mem_fun(&Handshake::close));
  std::for_each(Base::begin(), Base::end(), rak::call_delete<Handshake>());
}

void
HandshakeManager::erase(Handshake* handshake) {
  iterator itr = std::find(Base::begin(), Base::end(), handshake);

  if (itr == Base::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  Base::erase(itr);
}

bool
HandshakeManager::find(const SocketAddress& sa) {
  return std::find_if(Base::begin(), Base::end(),
		      rak::equal(sa, rak::on(std::mem_fun(&Handshake::get_peer),
					     std::mem_fun_ref<const torrent::SocketAddress&>(&PeerInfo::get_socket_address))))
    != Base::end();
}

void
HandshakeManager::add_incoming(SocketFd fd, const SocketAddress& sa) {
  if (!socketManager.received(fd, sa).is_valid())
    return;

  Base::push_back(new HandshakeIncoming(fd, PeerInfo("", sa, true), this));
}
  
void
HandshakeManager::add_outgoing(const SocketAddress& sa, TrackerInfo* info) {
  SocketFd fd = socketManager.open(sa, m_bindAddress);

  if (!fd.is_valid())
    return;

  Base::push_back(new HandshakeOutgoing(fd, this, PeerInfo("", sa, false), info));
}

void
HandshakeManager::receive_connected(Handshake* h) {
  erase(h);

  // TODO: Check that m_slotConnected actually points somewhere.
  m_slotConnected(h->get_fd(), h->info(), h->get_peer());

  h->set_fd(SocketFd());
  delete h;
}

void
HandshakeManager::receive_failed(Handshake* h) {
  erase(h);

  h->close();
  delete h;
}

}
