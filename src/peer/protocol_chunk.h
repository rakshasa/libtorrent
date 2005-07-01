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

#ifndef LIBTORRENT_NET_PROTOCOL_CHUNK_H
#define LIBTORRENT_NET_PROTOCOL_CHUNK_H

#include "data/piece.h"
#include "data/storage.h"

namespace torrent {

class SocketBase;

// TODO: Consider making this a template class that takes a functor.
class ProtocolChunk {
public:
  typedef Storage::Chunk         ChunkPtr;
  typedef StorageChunk::iterator ChunkPart;
  
  bool                is_done() const                                   { return m_position == m_piece.get_length(); }

  uint32_t            get_bytes_left() const                            { return m_piece.get_length() - m_position; }

  void                set_position(uint32_t p)                          { m_position = p; }

  const Piece&        get_piece() const                                 { return m_piece; }
  void                set_piece(const Piece& p)                         { m_piece = p; }

  ChunkPtr&           get_chunk()                                       { return m_chunk; }
  const ChunkPtr&     get_chunk() const                                 { return m_chunk; }
  void                set_chunk(const Storage::Chunk& c)                { m_chunk = c; }

  uint32_t            read(SocketBase* sock, uint32_t maxBytes);
  uint32_t            write(SocketBase* sock, uint32_t maxBytes);

private:
  // Returns true if we read 'length' bytes or finished the chunk part.
  inline bool         read_part(SocketBase* sock, ChunkPart c, uint32_t& left);
  inline bool         write_part(SocketBase* sock, ChunkPart c, uint32_t& left);

  ChunkPart           chunk_part();
  uint32_t            chunk_offset(ChunkPart c);
  uint32_t            chunk_length(ChunkPart c, uint32_t offset);

  uint32_t            m_position;
  Piece               m_piece;
  ChunkPtr            m_chunk;
};

inline ProtocolChunk::ChunkPart
ProtocolChunk::chunk_part() {
  return m_chunk->at_position(m_piece.get_offset() + m_position);
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
