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

#ifndef LIBTORRENT_DATA_STORAGE_CHUNK_PART_H
#define LIBTORRENT_DATA_STORAGE_CHUNK_PART_H

#include "memory_chunk.h"

namespace torrent {

class StorageChunkPart {
public:
  StorageChunkPart(const MemoryChunk& c, uint32_t pos) : m_chunk(c), m_position(pos) {}

  bool                is_valid() const                      { return m_chunk.is_valid(); }
  bool                is_contained(uint32_t p) const        { return p >= m_position && p < m_position + size(); }

  bool                has_permissions(int prot) const       { return m_chunk.has_permissions(prot); }

  void                clear();
  uint32_t            size() const                          { return m_chunk.size(); }

  MemoryChunk&        get_chunk()                           { return m_chunk; }
  uint32_t            get_position() const                  { return m_position; }

  uint32_t            incore_length(uint32_t pos);

private:
  MemoryChunk         m_chunk;
  uint32_t            m_position;
};

}

#endif
