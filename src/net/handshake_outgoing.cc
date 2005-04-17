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

#include <cstring>

#include "torrent/exceptions.h"

#include "handshake_outgoing.h"
#include "handshake_manager.h"
#include "poll.h"

namespace torrent {

HandshakeOutgoing::HandshakeOutgoing(SocketFd fd,
				     HandshakeManager* m,
				     const PeerInfo& p,
				     const std::string& infoHash,
				     const std::string& ourId) :
  Handshake(fd, m),
  m_state(CONNECTING) {
  
  m_peer = p;
  m_id = ourId;
  m_local = infoHash;

  Poll::write_set().insert(this);
  Poll::except_set().insert(this);
 
  m_buf[0] = 19;
  std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
  std::memset(&m_buf[20], 0, 8);
  std::memcpy(&m_buf[28], m_local.c_str(), 20);
  std::memcpy(&m_buf[48], m_id.c_str(), 20);
}
  
void
HandshakeOutgoing::read() {
  try {

  switch (m_state) {
  case READ_HEADER1:
    if (!recv1())
      return;

    if (m_hash != m_local)
      throw communication_error("Peer returned wrong download hash");

    m_pos = 0;
    m_state = READ_HEADER2;

  case READ_HEADER2:
    if (!recv2())
      return;

    m_manager->receive_connected(this);

    return;

  default:
    throw internal_error("HandshakeOutgoing::read() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}  

void
HandshakeOutgoing::write() {
  int error;

  try {

  switch (m_state) {
  case CONNECTING:
    error = m_fd.get_error();
 
    if (error)
      throw connection_error("Could not connect to client");
 
    m_state = WRITE_HEADER;
 
  case WRITE_HEADER:
//     if (!write_buf2(m_buf + m_pos, 68, m_pos))
//       return;
 
    m_pos += write_buf(m_buf + m_pos, 68 - m_pos);

    if (m_pos != 68)
      return;

    Poll::write_set().erase(this);
    Poll::read_set().insert(this);
 
    m_pos = 0;
    m_state = READ_HEADER1;
 
    return;

  default:
    throw internal_error("HandshakeOutgoing::write() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}

void
HandshakeOutgoing::except() {
  m_manager->receive_failed(this);
}

}

