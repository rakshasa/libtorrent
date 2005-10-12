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
#include <rak/error_number.h>

#include "torrent/exceptions.h"
#include "data/chunk_list.h"
#include "download/choke_manager.h"
#include "download/download_main.h"
#include "net/socket_base.h"

#include "peer_connection_base.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_download(NULL),
  
  m_down(new ProtocolRead()),
  m_up(new ProtocolWrite()),

  m_peerRate(600),

  m_downRate(30),
  m_downThrottle(throttleRead.end()),
  m_downStall(0),

  m_upRate(30),
  m_upThrottle(throttleWrite.end()),

  m_sendInterested(false),
  m_sendChoked(false),

  m_snubbed(false) {
}

PeerConnectionBase::~PeerConnectionBase() {
  if (!get_fd().is_valid())
    return;

  if (m_download == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  m_download->choke_manager()->disconnected(this);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);
  
  socketManager.close(get_fd());
  get_fd().clear();

  if (m_requestList.is_downloading())
    m_requestList.skip();

  down_chunk_release();
  up_chunk_release();

  m_requestList.cancel();

  remove_down_throttle();
  remove_up_throttle();

  m_down->set_state(ProtocolRead::INTERNAL_ERROR);
  m_up->set_state(ProtocolWrite::INTERNAL_ERROR);

  delete m_down;
  delete m_up;
  m_download = NULL;
}

bool
PeerConnectionBase::initialize(DownloadMain* download, const PeerInfo& p, SocketFd fd) {
  if (get_fd().is_valid())
    throw internal_error("Tried to re-set PeerConnection");

  set_fd(fd);

  m_peer = p;
  m_download = download;

  get_fd().set_throughput();

  m_requestList.set_delegator(m_download->delegator());
  m_requestList.set_bitfield(&m_bitfield.bitfield());

  if (m_download == NULL || !p.is_valid() || !get_fd().is_valid())
    throw internal_error("PeerConnectionSeed::set(...) recived bad input.");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(m_download->content()->chunk_total());

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  // Do this elsewhere.
  m_up->buffer()->reset();
  m_down->buffer()->reset();

  m_down->set_state(ProtocolRead::IDLE);
  m_up->set_state(ProtocolWrite::IDLE);
    
  m_timeLastRead = Timer::cache();

  initialize_custom();
}

void
PeerConnectionBase::load_down_chunk(const Piece& p) {
  m_downPiece = p;

  if (!m_download->content()->is_valid_piece(p))
    throw internal_error("Incoming pieces list contains a bad piece");
  
  if (m_downChunk.is_valid() && p.get_index() == m_downChunk->index())
    return;

  down_chunk_release();

  m_downChunk = m_download->chunk_list()->get(p.get_index(), true);
  
  if (!m_downChunk.is_valid())
    throw storage_error("File chunk write error: " + std::string(rak::error_number::current().c_str()));
}

void
PeerConnectionBase::load_up_chunk() {
  if (m_upChunk.is_valid() && m_upChunk->index() == m_upPiece.get_index())
    return;

  up_chunk_release();
  
  m_upChunk = m_download->chunk_list()->get(m_upPiece.get_index(), false);
  
  if (!m_upChunk.is_valid())
    throw storage_error("File chunk read error: " + std::string(rak::error_number::current().c_str()));
}

void
PeerConnectionBase::set_snubbed(bool v) {
  if (v == m_snubbed)
    return;

  bool wasUploadWanted = is_upload_wanted();
  m_snubbed = v;

  if (v) {
    if (wasUploadWanted)
      m_download->choke_manager()->set_not_interested(this);

  } else {
    if (is_upload_wanted())
      m_download->choke_manager()->set_interested(this);
  }
}

uint32_t
PeerConnectionBase::pipe_size() const {
  uint32_t s = m_downRate.rate();

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

void
PeerConnectionBase::receive_choke(bool v) {
  if (v == m_up->choked())
    throw internal_error("PeerConnectionBase::receive_choke(...) already set to the same state.");

  m_sendChoked = true;
  m_up->set_choked(v);
  m_timeLastChoked = Timer::cache();

  pollCustom->insert_write(this);
}

void
PeerConnectionBase::receive_throttle_down_activate() {
  pollCustom->insert_read(this);
}

void
PeerConnectionBase::receive_throttle_up_activate() {
  pollCustom->insert_write(this);
}

void
PeerConnectionBase::event_error() {
  m_download->connection_list()->erase(this);
}

bool
PeerConnectionBase::down_chunk() {
  if (!is_down_throttled())
    throw internal_error("PeerConnectionBase::down_chunk() tried to read a piece but is not in throttle list");

  if (!m_downChunk->chunk()->is_writable())
    throw internal_error("PeerConnectionBase::down_part() chunk not writable, permission denided");

  int quota = m_downThrottle->is_unlimited() ? std::numeric_limits<int>::max() : m_downThrottle->get_quota();

//   if (quota == 0)
//     throw internal_error("PeerConnectionBase::down_chunk() zero quota");

  if (quota < 0)
    throw internal_error("PeerConnectionBase::down_chunk() less-than zero quota");

  if (quota < 512) {
    pollCustom->remove_read(this);
    return false;
  }

  uint32_t count;
  uint32_t left = quota = std::min((uint32_t)quota, m_downPiece.get_length() - m_down->position());

  Chunk::MemoryArea memory;
  ChunkPart part = m_downChunk->chunk()->at_position(m_downPiece.get_offset() + m_down->position());

  do {
    memory = m_downChunk->chunk()->at_memory(m_downPiece.get_offset() + m_down->position(), part++);
    count = read_stream_throws(memory.first, std::min(left, memory.second));

    m_down->adjust_position(count);
    left -= count;

  } while (count == memory.second && left != 0);

  uint32_t bytes = quota - left;

  m_downRate.insert(bytes);
  m_downThrottle->used(bytes);

  throttleRead.get_rate_slow().insert(bytes);
  throttleRead.get_rate_quick().insert(bytes);
  m_download->down_rate().insert(bytes);

  return m_down->position() == m_downPiece.get_length();
}

bool
PeerConnectionBase::down_chunk_from_buffer() {
  uint32_t count, quota;
  uint32_t left = quota = std::min<uint32_t>(m_down->buffer()->remaining(),
					     m_downPiece.get_length() - m_down->position());

  Chunk::MemoryArea memory;
  ChunkPart part = m_downChunk->chunk()->at_position(m_downPiece.get_offset() + m_down->position());

  do {
    memory = m_downChunk->chunk()->at_memory(m_downPiece.get_offset() + m_down->position(), part++);
    count = std::min(left, memory.second);

    std::memcpy(memory.first, m_down->buffer()->position(), count);

    m_down->adjust_position(count);
    m_down->buffer()->move_position(count);
    left -= count;

  } while (left != 0);

  uint32_t bytes = quota - left;

  m_downRate.insert(bytes);

  if (is_down_throttled() && !m_downThrottle->is_unlimited())
    m_downThrottle->used(std::min<uint32_t>(bytes, m_downThrottle->get_quota()));

  throttleRead.get_rate_slow().insert(bytes);
  throttleRead.get_rate_quick().insert(bytes);
  m_download->down_rate().insert(bytes);

  return m_down->position() == m_downPiece.get_length();
}

bool
PeerConnectionBase::up_chunk() {
  if (!is_up_throttled())
    throw internal_error("PeerConnectionBase::up_chunk() tried to write a piece but is not in throttle list");

  if (!m_upChunk->chunk()->is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  int quota = m_upThrottle->is_unlimited() ? std::numeric_limits<int>::max() : m_upThrottle->get_quota();

//   if (quota == 0)
//     throw internal_error("PeerConnectionBase::up_chunk() zero quota");

  if (quota < 0)
    throw internal_error("PeerConnectionBase::up_chunk() less-than zero quota");

  if (quota < 512) {
    pollCustom->remove_write(this);
    return false;
  }

  uint32_t count;
  uint32_t left = quota = std::min((uint32_t)quota, m_upPiece.get_length() - m_up->position());

  Chunk::MemoryArea memory;
  ChunkPart part = m_upChunk->chunk()->at_position(m_upPiece.get_offset() + m_up->position());

  do {
    memory = m_upChunk->chunk()->at_memory(m_upPiece.get_offset() + m_up->position(), part++);
    count = write_stream_throws(memory.first, std::min(left, memory.second));

    m_up->adjust_position(count);
    left -= count;

  } while (count == memory.second && left != 0);

  uint32_t bytes = quota - left;

  m_upRate.insert(bytes);
  m_upThrottle->used(bytes);

  throttleWrite.get_rate_slow().insert(bytes);
  throttleWrite.get_rate_quick().insert(bytes);
  m_download->up_rate().insert(bytes);

  return m_up->position() == m_upPiece.get_length();
}

void
PeerConnectionBase::down_chunk_release() {
  if (m_downChunk.is_valid()) {
    m_download->chunk_list()->release(m_downChunk);
    m_downChunk.clear();
  }
}

void
PeerConnectionBase::up_chunk_release() {
  if (m_upChunk.is_valid()) {
    m_download->chunk_list()->release(m_upChunk);
    m_upChunk.clear();
  }
}

void
PeerConnectionBase::read_request_piece(const Piece& p) {
  PieceList::iterator itr = std::find(m_sendList.begin(), m_sendList.end(), p);
  
  if (itr != m_sendList.end())
    return;

  m_sendList.push_back(p);
  pollCustom->insert_write(this);
}

void
PeerConnectionBase::read_cancel_piece(const Piece& p) {
  PieceList::iterator itr = std::find(m_sendList.begin(), m_sendList.end(), p);
  
  if (itr != m_sendList.end() &&
      (itr != m_sendList.begin() || m_up->get_state() == ProtocolWrite::IDLE))
    m_sendList.erase(itr);
}  

void
PeerConnectionBase::read_buffer_move_unused() {
  uint32_t remaining = m_down->buffer()->remaining();
	
  std::memmove(m_down->buffer()->begin(), m_down->buffer()->position(), remaining);
	
  m_down->buffer()->reset_position();
  m_down->buffer()->set_end(remaining);
}

void
PeerConnectionBase::write_prepare_piece() {
  m_upPiece = m_sendList.front();

  // Move these checks somewhere else?
  if (!m_download->content()->is_valid_piece(m_upPiece) ||
      !m_download->content()->has_chunk(m_upPiece.get_index())) {
//     std::stringstream s;

//     s << "Peer requested a piece with invalid index or length/offset: "
//       << m_upPiece.get_index() << ' '
//       << m_upPiece.get_length() << ' '
//       << m_upPiece.get_offset();

    throw communication_error("Peer requested a piece with invalid index or length/offset.");
  }
      
  m_up->write_piece(m_upPiece);
}

void
PeerConnectionBase::write_finished_piece() {
  if (m_sendList.empty() || m_sendList.front() != m_upPiece)
    throw internal_error("ProtocolWrite::WRITE_PIECE found the wrong piece in the send queue.");

  // Do we need to check that this is the right piece?
  m_sendList.pop_front();
	
  if (m_sendList.empty())
    up_chunk_release();
}

bool
PeerConnectionBase::read_bitfield_body() {
  // We're guaranteed that we still got bytes remaining to be
  // read of the bitfield.
  m_down->adjust_position(read_stream_throws(m_bitfield.begin() + m_down->position(),
					     m_bitfield.size_bytes() - m_down->position()));
	
  return m_down->position() == m_bitfield.size_bytes();
}

// 'msgLength' is the length of the message, not how much we got in
// the buffer.
bool
PeerConnectionBase::read_bitfield_from_buffer(uint32_t msgLength) {
  if (msgLength != m_bitfield.size_bytes())
    throw network_error("Received invalid bitfield size.");

  uint32_t copyLength = std::min((uint32_t)m_down->buffer()->remaining(), msgLength);

  std::memcpy(m_bitfield.begin(), m_down->buffer()->position(), copyLength);

  m_down->buffer()->move_position(copyLength);
  m_down->set_position(copyLength);

  return copyLength == msgLength;
}

bool
PeerConnectionBase::write_bitfield_body() {
  m_up->adjust_position(write_stream_throws(m_download->content()->bitfield().begin() + m_up->position(),
					    m_download->content()->bitfield().size_bytes() - m_up->position()));

  return m_up->position() == m_bitfield.size_bytes();
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
PeerConnectionBase::should_request() {
  if (m_down->choked() ||
      !m_up->interested() ||
      m_down->get_state() == ProtocolRead::READ_SKIP_PIECE)
    return false;

  else if (!m_download->get_endgame())
    return true;
  else
    // We check if the peer is stalled, if it is not then we should
    // request. If the peer is stalled then we only request if the
    // download rate is below a certain value.
    return m_downStall <= 1 || m_download->down_rate().rate() < (10 << 10);
}

bool
PeerConnectionBase::try_request_pieces() {
  if (m_requestList.empty())
    m_downStall = 0;

  int pipeSize = pipe_size();
  bool success = false;

  while (m_requestList.size() < pipeSize && m_up->can_write_request()) {
    const Piece* p = m_requestList.delegate();

    if (p == NULL)
      break;

    if (!m_download->content()->is_valid_piece(*p) || !m_bitfield[p->get_index()])
      throw internal_error("PeerConnectionBase::try_request_pieces() tried to use an invalid piece");

    m_up->write_request(*p);

    success = true;
  }

  return success;
}

void
PeerConnectionBase::set_remote_interested() {
  if (m_down->interested() || m_bitfield.all_set())
    return;

  m_down->set_interested(true);

  if (is_upload_wanted())
    m_download->choke_manager()->set_interested(this);
}

void
PeerConnectionBase::set_remote_not_interested() {
  if (!m_down->interested())
    return;

  bool wasUploadWanted = is_upload_wanted();

  m_down->set_interested(false);

  if (wasUploadWanted)
    m_download->choke_manager()->set_not_interested(this);
}

}
