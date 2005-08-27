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

#include <limits>

#include "torrent/exceptions.h"
#include "data/chunk_list_node.h"
#include "download/download_main.h"
#include "net/socket_base.h"

#include "peer_connection_base.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_download(NULL),
  
  m_read(new ProtocolRead()),
  m_write(new ProtocolWrite()),

  m_peerRate(600),

  m_readRate(30),
  m_readThrottle(throttleRead.end()),
  m_readStall(0),

  m_writeRate(30),
  m_writeThrottle(throttleWrite.end()),

  m_snubbed(false),

  m_downChunk(NULL),
  m_upChunk(NULL) {
}

PeerConnectionBase::~PeerConnectionBase() {
  if (m_requestList.is_downloading())
    m_requestList.skip();

  if (m_downChunk != NULL)
    m_download->content()->release_chunk(m_downChunk);

  if (m_upChunk != NULL)
    m_download->content()->release_chunk(m_upChunk);

  m_downChunk = NULL;
  m_upChunk = NULL;

  m_requestList.cancel();

  remove_read_throttle();
  remove_write_throttle();

  m_read->set_state(ProtocolRead::INTERNAL_ERROR);
  m_write->set_state(ProtocolWrite::INTERNAL_ERROR);

  delete m_read;
  delete m_write;
}

void
PeerConnectionBase::load_read_chunk(const Piece& p) {
  m_downPiece = p;

  if (!m_download->content()->is_valid_piece(p))
    throw internal_error("Incoming pieces list contains a bad piece");
  
  if (m_downChunk != NULL && p.get_index() == m_downChunk->index())
    return;

  if (m_downChunk != NULL)
    m_download->content()->release_chunk(m_downChunk);

  m_downChunk = m_download->content()->get_chunk(p.get_index(), MemoryChunk::prot_read | MemoryChunk::prot_write);
  
  if (m_downChunk == NULL)
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
    pollCustom->remove_write(this);
    return false;
  }

  uint32_t bytes = up_chunk(quota);

  m_writeRate.insert(bytes);
  m_writeThrottle->used(bytes);

  throttleWrite.get_rate_slow().insert(bytes);
  throttleWrite.get_rate_quick().insert(bytes);
  m_download->get_write_rate().insert(bytes);

  return m_upChunkPosition == m_upPiece.get_length();
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
    pollCustom->remove_read(this);
    return false;
  }

  uint32_t bytes = down_chunk(quota);

  m_readRate.insert(bytes);
  m_readThrottle->used(bytes);

  throttleRead.get_rate_slow().insert(bytes);
  throttleRead.get_rate_quick().insert(bytes);
  m_download->get_read_rate().insert(bytes);

  return m_downChunkPosition == m_downPiece.get_length();
}

uint32_t
PeerConnectionBase::pipe_size() const {
  uint32_t s = m_readRate.rate();

  if (!m_download->get_endgame())
    if (s < 50000)
      return std::max((uint32_t)2, (s + 2000) / 2000);
    else
      return std::min((uint32_t)200, (s + 160000) / 4000);

  else
    if (s < 4000)
      return 1;
    else
      return std::min((uint32_t)80, (s + 32000) / 8000);
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
PeerConnectionBase::should_request() {
  if (!m_download->get_endgame())
    return true;
  else
    // We check if the peer is stalled, if it is not then we should
    // request. If the peer is stalled then we only request if the
    // download rate is below a certain value.
    return m_readStall <= 1 || m_download->get_read_rate().rate() < (10 << 10);
}

void
PeerConnectionBase::receive_throttle_read_activate() {
  pollCustom->insert_read(this);
}

void
PeerConnectionBase::receive_throttle_write_activate() {
  pollCustom->insert_write(this);
}

inline bool
PeerConnectionBase::down_chunk_part(ChunkPart c, uint32_t& left) {
  if (!c->chunk().is_valid())
    throw internal_error("PeerConnectionBase::down_part() did not get a valid chunk");
  
  uint32_t offset = m_downPiece.get_offset() + m_downChunkPosition - c->position();
  uint32_t length = std::min(std::min(m_downPiece.get_length() - m_downChunkPosition,
				      c->size() - offset),
			     left);

  uint32_t done = read_buf(c->chunk().begin() + offset, length);

  m_downChunkPosition += done;
  left -= done;

  return done == length;
}

uint32_t
PeerConnectionBase::down_chunk(uint32_t maxBytes) {
  if (!m_downChunk->chunk()->is_writable())
    throw internal_error("PeerConnectionBase::down_part() chunk not writable, permission denided");

  uint32_t left = maxBytes = std::min(maxBytes, m_downPiece.get_length() - m_downChunkPosition);

  ChunkPart c = m_downChunk->chunk()->at_position(m_downPiece.get_offset() + m_downChunkPosition);

  while (down_chunk_part(c++, left) && left != 0)
    if (c == m_downChunk->chunk()->end())
      throw internal_error("PeerConnectionBase::down() reached end of chunk part list");

  return maxBytes - left;
}

inline bool
PeerConnectionBase::up_chunk_part(ChunkPart c, uint32_t& left) {
  if (!c->chunk().is_valid())
    throw internal_error("ProtocolChunk::write_part() did not get a valid chunk");
  
  uint32_t offset = m_upPiece.get_offset() + m_upChunkPosition - c->position();
  uint32_t length = std::min(std::min(m_upPiece.get_length() - m_upChunkPosition,
				      c->size() - offset),
			     left);

  uint32_t done = write_buf(c->chunk().begin() + offset, length);

  m_upChunkPosition += done;
  left -= done;

  return done == length;
}

uint32_t
PeerConnectionBase::up_chunk(uint32_t maxBytes) {
  if (!m_upChunk->chunk()->is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  uint32_t left = maxBytes = std::min(maxBytes, m_upPiece.get_length() - m_upChunkPosition);

  ChunkPart c = m_upChunk->chunk()->at_position(m_upPiece.get_offset() + m_upChunkPosition);

  while (up_chunk_part(c++, left) && left != 0)
    if (c == m_upChunk->chunk()->end())
      throw internal_error("PeerConnectionBase::up_chunk(...) reached end of chunk part list.");

  return maxBytes - left;
}

}
