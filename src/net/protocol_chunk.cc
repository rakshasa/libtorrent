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

#include "protocol_chunk.h"
#include "socket_base.h"

namespace torrent {

uint32_t
ProtocolChunk::read(SocketBase* sock, uint32_t maxBytes) {
  uint32_t left = maxBytes = std::min(maxBytes, m_piece.get_length() - m_position);

  ChunkPart c = chunk_part();

  while (read_part(sock, c++, left) && left != 0)
    if (c == m_chunk->end())
      throw internal_error("ProtocolChunk::read() reached end of chunk part list");

  return maxBytes - left;
}

uint32_t
ProtocolChunk::write(SocketBase* sock, uint32_t maxBytes) {
  uint32_t left = maxBytes = std::min(maxBytes, m_piece.get_length() - m_position);

  ChunkPart c = chunk_part();

  while (write_part(sock, c++, left) && left != 0)
    if (c == m_chunk->end())
      throw internal_error("ProtocolChunk::read() reached end of chunk part list");

  return maxBytes - left;
}

inline bool
ProtocolChunk::read_part(SocketBase* sock, ChunkPart c, uint32_t& left) {
  if (!c->get_chunk().is_valid())
    throw internal_error("ProtocolChunk::read_part() did not get a valid chunk");
  
  if (!c->get_chunk().is_writable())
    throw internal_error("ProtocolChunk::read_part() chunk not writable, permission denided");

  uint32_t offset = chunk_offset(c);
  uint32_t length = std::min(chunk_length(c, offset), left);

  uint32_t done = sock->read_buf(c->get_chunk().begin() + offset, length);

  m_position += done;
  left -= done;

  return done == length;
}

inline bool
ProtocolChunk::write_part(SocketBase* sock, ChunkPart c, uint32_t& left) {
  if (!c->get_chunk().is_valid())
    throw internal_error("ProtocolChunk::read_part() did not get a valid chunk");
  
  if (!c->get_chunk().is_writable())
    throw internal_error("ProtocolChunk::read_part() chunk not writable, permission denided");

  uint32_t offset = chunk_offset(c);
  uint32_t length = std::min(chunk_length(c, offset), left);

  uint32_t done = sock->write_buf(c->get_chunk().begin() + offset, length);

  m_position += done;
  left -= done;

  return done == length;
}

}
