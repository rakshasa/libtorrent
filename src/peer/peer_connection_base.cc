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

#include <limits>

#include "torrent/exceptions.h"
#include "download/download_net.h"
#include "download/download_state.h"
#include "net/socket_base.h"

#include "peer_connection_base.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_state(NULL),
  m_net(NULL),
  
  m_read(new ProtocolRead()),
  m_write(new ProtocolWrite()),

  m_readThrottle(throttleRead.end()),
  m_writeThrottle(throttleWrite.end()),
  m_snubbed(false) {
}

PeerConnectionBase::~PeerConnectionBase() {
  remove_read_throttle();
  remove_write_throttle();
}

void
PeerConnectionBase::load_read_chunk(const Piece& p) {
  m_readChunk.set_piece(p);

  if (m_readChunk.get_chunk().is_valid() && p.get_index() == m_readChunk.get_chunk()->get_index())
    return;

  if (!m_state->get_content().is_valid_piece(p))
    throw internal_error("Incoming pieces list contains a bad piece");
  
  m_readChunk.set_chunk(m_state->get_content().get_storage().get_chunk(p.get_index(), MemoryChunk::prot_read | MemoryChunk::prot_write));
  
  if (!m_readChunk.get_chunk().is_valid())
    throw storage_error("Could not create a valid chunk");
}

bool
PeerConnectionBase::write_chunk() {
  if (!is_write_throttled())
    throw internal_error("PeerConnectionBase::write_chunk() tried to write a piece but is not in throttle list");

  int quota = m_writeThrottle->is_unlimited() ? std::numeric_limits<int>::max() : m_writeThrottle->get_quota();

//   if (quota == 0)
//     throw internal_error("PeerConnectionBase::write_chunk() zero quota");

  if (quota < 0)
    throw internal_error("PeerConnectionBase::write_chunk() less-than zero quota");

  if (quota < 512) {
    PollManager::write_set().erase(this);
    return false;
  }

  uint32_t bytes = m_writeChunk.write(this, quota);

  m_writeRate.insert(bytes);
  m_writeThrottle->used(bytes);

  throttleWrite.get_rate_slow().insert(bytes);
  throttleWrite.get_rate_quick().insert(bytes);
  m_net->get_write_rate().insert(bytes);

  return m_writeChunk.is_done();
}

bool
PeerConnectionBase::read_chunk() {
  if (!is_read_throttled())
    throw internal_error("PeerConnectionBase::read_chunk() tried to read a piece but is not in throttle list");

  int quota = m_readThrottle->is_unlimited() ? std::numeric_limits<int>::max() : m_readThrottle->get_quota();

//   if (quota == 0)
//     throw internal_error("PeerConnectionBase::read_chunk() zero quota");

  if (quota < 0)
    throw internal_error("PeerConnectionBase::read_chunk() less-than zero quota");

  if (quota < 512) {
    PollManager::read_set().erase(this);
    return false;
  }

  uint32_t bytes = m_readChunk.read(this, quota);

  m_readRate.insert(bytes);
  m_readThrottle->used(bytes);

  throttleRead.get_rate_slow().insert(bytes);
  throttleRead.get_rate_quick().insert(bytes);
  m_net->get_read_rate().insert(bytes);

  return m_readChunk.is_done();
}

void
PeerConnectionBase::receive_throttle_read_activate() {
  PollManager::read_set().insert(this);
}

void
PeerConnectionBase::receive_throttle_write_activate() {
  PollManager::write_set().insert(this);
}

}
