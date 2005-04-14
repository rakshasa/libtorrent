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

#include "config.h"

#include <algo/algo.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "handshake_manager.h"
#include "handshake_incoming.h"
#include "handshake_outgoing.h"
#include "peer_info.h"
#include "socket_address.h"

using namespace algo;

namespace torrent {

void
HandshakeManager::add_incoming(int fd,
			       const std::string& dns,
			       uint16_t port) {
  m_handshakes.push_back(new HandshakeIncoming(fd, this, PeerInfo("", dns, port)));
  m_size++;
}
  
void
HandshakeManager::add_outgoing(const PeerInfo& p,
			       const std::string& infoHash,
			       const std::string& ourId) {
  SocketFd fd;
  SocketAddress sa;

  if (!sa.create(p.get_dns(), p.get_port()) ||
      !(fd = make_socket(sa)).is_valid())
    return;

  m_handshakes.push_back(new HandshakeOutgoing(fd, this, p, infoHash, ourId));
  m_size++;
}

void
HandshakeManager::clear() {
  std::for_each(m_handshakes.begin(), m_handshakes.end(),
		branch(call_member(&Handshake::close), delete_on()));

  m_handshakes.clear();
}

uint32_t
HandshakeManager::get_size_hash(const std::string& hash) {
  return std::count_if(m_handshakes.begin(), m_handshakes.end(), eq(call_member(&Handshake::get_hash), ref(hash)));
}

bool
HandshakeManager::has_peer(const PeerInfo& p) {
  return std::find_if(m_handshakes.begin(), m_handshakes.end(),
		      call_member(call_member(&Handshake::get_peer), &PeerInfo::is_same_host, ref(p)))
    != m_handshakes.end();
}

void
HandshakeManager::receive_connected(Handshake* h) {
  remove(h);

  m_slotConnected(h->get_fd(), h->get_hash(), h->get_peer());

  h->set_fd(-1);
  delete h;
}

void
HandshakeManager::receive_failed(Handshake* h) {
  remove(h);

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

SocketFd
HandshakeManager::make_socket(SocketAddress& sa) {
  SocketFd fd;

  if (fd.open() && (!fd.set_nonblock()) || !fd.connect(sa)) {
    fd.close();
    fd.clear();
  }

  return fd;
}

}
