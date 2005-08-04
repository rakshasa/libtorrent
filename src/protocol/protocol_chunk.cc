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

#include "net/socket_stream.h"
#include "torrent/exceptions.h"

#include "protocol_chunk.h"

namespace torrent {

uint32_t
ProtocolChunk::read(SocketStream* sock, uint32_t maxBytes) {
  uint32_t left = maxBytes = std::min(maxBytes, m_piece.get_length() - m_position);

  ChunkPart c = chunk_part();

  while (read_part(sock, c++, left) && left != 0)
    if (c == m_chunk->end())
      throw internal_error("ProtocolChunk::read() reached end of chunk part list");

  return maxBytes - left;
}

uint32_t
ProtocolChunk::write(SocketStream* sock, uint32_t maxBytes) {
  uint32_t left = maxBytes = std::min(maxBytes, m_piece.get_length() - m_position);

  ChunkPart c = chunk_part();

  while (write_part(sock, c++, left) && left != 0)
    if (c == m_chunk->end())
      throw internal_error("ProtocolChunk::read() reached end of chunk part list");

  return maxBytes - left;
}

inline bool
ProtocolChunk::read_part(SocketStream* sock, ChunkPart c, uint32_t& left) {
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
ProtocolChunk::write_part(SocketStream* sock, ChunkPart c, uint32_t& left) {
  if (!c->get_chunk().is_valid())
    throw internal_error("ProtocolChunk::write_part() did not get a valid chunk");
  
  if (!c->get_chunk().is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  uint32_t offset = chunk_offset(c);
  uint32_t length = std::min(chunk_length(c, offset), left);

  uint32_t done = sock->write_buf(c->get_chunk().begin() + offset, length);

  m_position += done;
  left -= done;

  return done == length;
}

}
