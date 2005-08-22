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

  m_snubbed(false) {
}

PeerConnectionBase::~PeerConnectionBase() {
  if (m_requestList.is_downloading())
    m_requestList.skip();

  if (m_readChunk.get_chunk() != NULL)
    m_download->state()->get_content().release_chunk(m_readChunk.get_chunk());

  if (m_writeChunk.get_chunk() != NULL)
    m_download->state()->get_content().release_chunk(m_writeChunk.get_chunk());

  m_readChunk.set_chunk(NULL);
  m_writeChunk.set_chunk(NULL);

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
  m_readChunk.set_piece(p);

  if (m_readChunk.get_chunk()->is_valid() && p.get_index() == m_readChunk.get_chunk()->chunk()->get_index())
    return;

  if (!m_download->state()->get_content().is_valid_piece(p))
    throw internal_error("Incoming pieces list contains a bad piece");
  
  m_readChunk.set_chunk(m_download->state()->get_content().get_chunk(p.get_index(), MemoryChunk::prot_read | MemoryChunk::prot_write));
  
  if (!m_readChunk.get_chunk()->is_valid())
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

  uint32_t bytes = m_writeChunk.write(this, quota);

  m_writeRate.insert(bytes);
  m_writeThrottle->used(bytes);

  throttleWrite.get_rate_slow().insert(bytes);
  throttleWrite.get_rate_quick().insert(bytes);
  m_download->get_write_rate().insert(bytes);

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
    pollCustom->remove_read(this);
    return false;
  }

  uint32_t bytes = m_readChunk.read(this, quota);

  m_readRate.insert(bytes);
  m_readThrottle->used(bytes);

  throttleRead.get_rate_slow().insert(bytes);
  throttleRead.get_rate_quick().insert(bytes);
  m_download->get_read_rate().insert(bytes);

  return m_readChunk.is_done();
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

}
