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
#include "download/download_net.h"
#include "download/download_state.h"
#include "net/socket_base.h"

#include "peer_connection_base.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_state(NULL),
  m_net(NULL),
  
  m_readThrottle(throttleRead.end()),
  m_writeThrottle(throttleWrite.end()) {
}

PeerConnectionBase::~PeerConnectionBase() {
  remove_read_throttle();
  remove_write_throttle();
}

void
PeerConnectionBase::load_read_chunk(const Piece& p) {
  m_readPiece = p;

  if (m_read.get_chunk().is_valid() && p.get_index() == m_read.get_chunk()->get_index())
    return;

  if (!m_state->get_content().is_valid_piece(p))
    throw internal_error("Incoming pieces list contains a bad piece");
  
  m_read.get_chunk() = m_state->get_content().get_storage().get_chunk(p.get_index(), MemoryChunk::prot_read | MemoryChunk::prot_write);
  
  if (!m_read.get_chunk().is_valid())
    throw storage_error("Could not create a valid chunk");
}

void
PeerConnectionBase::receive_throttle_read_activate() {
  Poll::read_set().insert(this);
}

void
PeerConnectionBase::receive_throttle_write_activate() {
  Poll::write_set().insert(this);
}

}
