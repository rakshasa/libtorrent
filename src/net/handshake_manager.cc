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

#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "handshake_manager.h"
#include "handshake_incoming.h"
#include "handshake_outgoing.h"
#include "peer/peer_info.h"

#include "manager.h"
#include "socket_address.h"

namespace torrent {

void
HandshakeManager::add_incoming(SocketFd fd, const SocketAddress& sa) {
  if (!socketManager.received(fd, sa).is_valid())
    return;

  m_handshakes.push_back(new HandshakeIncoming(fd, PeerInfo("", sa), this));
  m_size++;
}
  
void
HandshakeManager::add_outgoing(const PeerInfo& p,
			       const std::string& infoHash,
			       const std::string& ourId) {
  SocketFd fd;

  if (!(fd = socketManager.open(p.get_socket_address(), m_bindAddress)).is_valid())
    return;

  m_handshakes.push_back(new HandshakeOutgoing(fd, this, p, infoHash, ourId));
  m_size++;
}

void
HandshakeManager::clear() {
  std::for_each(m_handshakes.begin(), m_handshakes.end(), std::mem_fun(&Handshake::close));
  std::for_each(m_handshakes.begin(), m_handshakes.end(), rak::call_delete<Handshake>());

  m_handshakes.clear();
}

uint32_t
HandshakeManager::get_size_hash(const std::string& hash) {
  return std::count_if(m_handshakes.begin(), m_handshakes.end(),
		       rak::equal(hash, std::mem_fun(&Handshake::get_hash)));
}

bool
HandshakeManager::has_peer(const PeerInfo& p) {
  return std::find_if(m_handshakes.begin(), m_handshakes.end(),
		      rak::on(std::mem_fun(&Handshake::get_peer),
			      rak::bind2nd(std::mem_fun_ref(&PeerInfo::is_same_host), p)))
    != m_handshakes.end();
}

void
HandshakeManager::receive_connected(Handshake* h) {
  remove(h);

  h->clear_poll();

  // TODO: Check that m_slotConnected actually points somewhere.
  m_slotConnected(h->get_fd(), h->get_hash(), h->get_peer());

  h->set_fd(SocketFd());
  delete h;
}

void
HandshakeManager::receive_failed(Handshake* h) {
  remove(h);

  h->clear_poll(); // This should be here, might need to remove to debug.
  h->close();
  delete h;
}

void
HandshakeManager::remove(Handshake* h) {
  HandshakeList::iterator itr = std::find(m_handshakes.begin(), m_handshakes.end(), h);

  if (itr == m_handshakes.end())
    throw internal_error("HandshakeManager::remove(...) could not find Handshake");

  m_handshakes.erase(itr);
  m_size--;
}

}
