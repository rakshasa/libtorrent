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

#ifndef LIBTORRENT_NET_PROTOCOL_CHUNK_H
#define LIBTORRENT_NET_PROTOCOL_CHUNK_H

#include "data/piece.h"
#include "data/storage_chunk.h"
#include "data/chunk_list_node.h"

namespace torrent {

class SocketStream;

// TODO: Consider making this a template class that takes a functor.
class ProtocolChunk {
public:
  typedef StorageChunk::iterator ChunkPart;
  
  ProtocolChunk() : m_chunk(NULL) {}

  bool                is_done() const                                   { return m_position == m_piece.get_length(); }

  uint32_t            get_bytes_left() const                            { return m_piece.get_length() - m_position; }

  void                set_position(uint32_t p)                          { m_position = p; }

  const Piece&        get_piece() const                                 { return m_piece; }
  void                set_piece(const Piece& p)                         { m_piece = p; }

  ChunkListNode*      get_chunk()                                       { return m_chunk; }
  ChunkListNode*      get_chunk() const                                 { return m_chunk; }
  void                set_chunk(ChunkListNode* node)                    { m_chunk = node; }

  uint32_t            read(SocketStream* sock, uint32_t maxBytes);
  uint32_t            write(SocketStream* sock, uint32_t maxBytes);

private:
  // Returns true if we read 'length' bytes or finished the chunk part.
  inline bool         read_part(SocketStream* sock, ChunkPart c, uint32_t& left);
  inline bool         write_part(SocketStream* sock, ChunkPart c, uint32_t& left);

  ChunkPart           chunk_part();
  uint32_t            chunk_offset(ChunkPart c);
  uint32_t            chunk_length(ChunkPart c, uint32_t offset);

  uint32_t            m_position;
  Piece               m_piece;
  ChunkListNode*      m_chunk;
};

inline ProtocolChunk::ChunkPart
ProtocolChunk::chunk_part() {
  return m_chunk->chunk()->at_position(m_piece.get_offset() + m_position);
}  

inline uint32_t
ProtocolChunk::chunk_offset(ChunkPart c) {
  return m_piece.get_offset() + m_position - c->get_position();
}

inline uint32_t
ProtocolChunk::chunk_length(ChunkPart c, uint32_t offset) {
  return std::min(m_piece.get_length() - m_position, c->size() - offset);
}

}

#endif
