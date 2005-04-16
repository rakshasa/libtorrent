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

#include "torrent/exceptions.h"

#include "handshake_incoming.h"
#include "handshake_manager.h"
#include "poll.h"

namespace torrent {

HandshakeIncoming::HandshakeIncoming(SocketFd fd, HandshakeManager* m, const PeerInfo& p) :
  Handshake(fd, m),
  m_state(READ_HEADER1) {

  m_peer = p;

  m_fd.set_nonblock();

  Poll::read_set().insert(this);
  Poll::except_set().insert(this);
}

void
HandshakeIncoming::read() {
  try {

  switch (m_state) {
  case READ_HEADER1:
    if (!recv1())
      return;

    if ((m_id = m_manager->get_download_id(m_hash)).length() == 0)
      throw close_connection();
    
    m_buf[0] = 19;
    std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
    std::memset(&m_buf[20], 0, 8);
    std::memcpy(&m_buf[28], m_hash.c_str(), 20);
    std::memcpy(&m_buf[48], m_id.c_str(), 20);

    m_pos = 0;
    m_state = WRITE_HEADER;

    Poll::read_set().erase(this);
    Poll::write_set().insert(this);

    return;

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
HandshakeIncoming::write() {
  try {
  switch (m_state) {
  case WRITE_HEADER:
    if (!write_buf2(m_buf + m_pos, 68, m_pos))
      return;
 
    Poll::write_set().erase(this);
    Poll::read_set().insert(this);
 
    m_pos = 0;
    m_state = READ_HEADER2;
 
    return;

  default:
    throw internal_error("HandshakeOutgoing::write() called in wrong state");
  }

  } catch (network_error e) {
    m_manager->receive_failed(this);
  }
}

void
HandshakeIncoming::except() {
  m_manager->receive_failed(this);
}

}

