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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <algo/algo.h>
#include <netinet/in.h>
#include <sstream>

#include "torrent/exceptions.h"
#include "download/download_state.h"
#include "download/download_net.h"

#include "peer_connection.h"

using namespace algo;

// This file is for functions that does not directly relate to the
// protocol logic. Or the split might be completely arbitary...

namespace torrent {

PeerConnection::PeerConnection() :
  m_shutdown(false),
  m_stallCount(0),

  m_download(NULL),
  m_net(NULL),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true),
  
  m_taskKeepAlive(sigc::mem_fun(*this, &PeerConnection::task_keep_alive)),
  m_taskSendChoke(sigc::mem_fun(*this, &PeerConnection::task_send_choke)),
  m_taskStall(sigc::mem_fun(*this, &PeerConnection::task_stall))
{
}

PeerConnection::~PeerConnection() {
  if (m_download) {
    if (m_requests.is_downloading())
      m_requests.skip();

    m_requests.cancel();

    if (m_down.state != READ_BITFIELD)
      m_download->get_bitfield_counter().dec(m_bitfield.get_bitfield());
  }

  if (m_fd.is_valid())
    m_fd.close();
}

// TODO: Make this a while loop so we spit out as much of the piece as we can this work cycle.
bool
PeerConnection::writeChunk(unsigned int maxBytes) {
  if (m_up.m_pos2 >= (1 << 17))
    throw internal_error("PeerConnection::writeChunk(...) m_pos2 bork");

  uint32_t offset = m_sends.front().get_offset() + m_up.m_pos2;
  StorageChunk::iterator part = m_up.data->at_position(offset);

  offset -= part->get_position();

  uint32_t length = std::min(m_sends.front().get_length() - m_up.m_pos2, part->size() - offset);

  if (length > (1 << 17) || length == 0 )
    throw internal_error("PeerConnection::writeChunk(...) length bork");

  if (offset > part->size())
    throw internal_error("PeerConnection::writeChunk(...) offset bork");

  if ((offset + length) > part->size())
    throw internal_error("PeerConnection::writeChunk(...) offset+length bork");

  m_up.m_pos2 += write_buf(part->get_chunk().begin() + offset, std::min(length, maxBytes));

  return m_up.m_pos2 == m_sends.front().get_length();
}

// TODO: Handle file boundaries better.
bool
PeerConnection::readChunk() {
  if (m_down.m_pos2 > (1 << 17) + 9)
    throw internal_error("Really bad read position for buffer");
  
  const Piece& p = m_requests.get_piece();
  StorageChunk::iterator part = m_down.data->at_position(p.get_offset() + m_down.m_pos2);

  unsigned int offset = p.get_offset() + m_down.m_pos2 - part->get_position();
  
  if (!part->get_chunk().is_valid())
    throw internal_error("PeerConnection::readChunk() did not get a valid chunk");
  
  if (!part->get_chunk().is_writable())
    throw internal_error("PeerConnection::readChunk() chunk not writable, permission denided");
  
  m_down.m_pos2 += read_buf(part->get_chunk().begin() + offset,
			    std::min(p.get_length() - m_down.m_pos2, part->size() - offset));

  return m_down.m_pos2 == p.get_length();
}
  
void PeerConnection::bufCmd(Protocol cmd, unsigned int length) {
  m_up.m_buf.write_uint32(length);
  m_up.m_buf.write_uint8(cmd);

  if (m_up.m_buf.size() > m_up.m_buf.reserved())
    throw internal_error("PeerConnection write buffer to small");

  m_up.lastCommand = cmd;
}

void PeerConnection::bufW32(uint32_t v) {
  m_up.m_buf.write_uint32(v);

  if (m_up.m_buf.size() > m_up.m_buf.reserved())
    throw internal_error("PeerConnection tried to write beyond scope of packet");
}

uint32_t PeerConnection::bufR32(bool peep) {
  if (m_down.m_buf.size() + 4 > m_down.m_buf.reserved())
    throw internal_error("PeerConnection tried to read beyond scope of packet");

  return peep ? m_down.m_buf.peek_uint32() : m_down.m_buf.read_uint32();
}
 
void
PeerConnection::load_chunk(int index, Sub& sub) {
  if (sub.data.is_valid() && index == sub.data->get_index())
    return;

  if (index < 0 || index >= (signed)m_download->get_chunk_total())
    throw internal_error("Incoming pieces list contains a bad index value");
  
  sub.data = m_download->get_content().get_storage().get_chunk(index, MemoryChunk::prot_read | MemoryChunk::prot_write);
  
  if (!sub.data.is_valid())
    throw storage_error("Could not create a valid chunk");
}

bool
PeerConnection::request_piece() {
  const Piece* p;

  if ((p = m_requests.delegate()) == NULL)
    return false;

  if (p->get_length() > (1 << 17) ||
      p->get_length() == 0 ||
      p->get_length() + p->get_offset() >
      
      ((unsigned)p->get_index() + 1 != m_download->get_chunk_total() ||
       !(m_download->get_content().get_size() % m_download->get_content().get_storage().get_chunk_size()) ?
       
       m_download->get_content().get_storage().get_chunk_size() :
       (m_download->get_content().get_size() % m_download->get_content().get_storage().get_chunk_size()))) {
    
    std::stringstream s;
    
    s << "Tried to request a piece with invalid length or offset: "
      << p->get_length() << ' '
      << p->get_offset();
    
    throw internal_error(s.str());
  }

  if (p->get_index() < 0 ||
      p->get_index() >= (int)m_bitfield.size_bits() ||
      !m_bitfield[p->get_index()])
    throw internal_error("Delegator gave us a piece with invalid range or not in peer");

  bufCmd(REQUEST, 13);
  bufW32(p->get_index());
  bufW32(p->get_offset());
  bufW32(p->get_length());

  return true;
}

bool PeerConnection::chokeDelayed() {
  return m_sendChoked || m_taskSendChoke.is_scheduled();
}

void PeerConnection::choke(bool v) {
  if (m_up.choked != v) {
    m_sendChoked = true;
    m_up.choked = v;

    insert_write();
  }
}

void
PeerConnection::update_interested() {
  if (m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_down.interested;
    m_down.interested = true;
  } else {
    m_sendInterested = m_down.interested;
    m_down.interested = false;
  }
}

} // namespace torrent
