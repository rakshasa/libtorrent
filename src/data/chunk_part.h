// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#ifndef LIBTORRENT_DATA_STORAGE_CHUNK_PART_H
#define LIBTORRENT_DATA_STORAGE_CHUNK_PART_H

#include "memory_chunk.h"

namespace torrent {

class ChunkPart {
public:
  typedef enum {
    MAPPED_MMAP,
    MAPPED_STATIC
  } mapped_type;

  ChunkPart(mapped_type mapped, const MemoryChunk& c, uint32_t pos) :
    m_mapped(mapped), m_chunk(c), m_position(pos) {}

  bool                is_valid() const                      { return m_chunk.is_valid(); }
  bool                is_contained(uint32_t p) const        { return p >= m_position && p < m_position + size(); }

  void                clear();

  mapped_type         mapped() const                        { return m_mapped; }

  MemoryChunk&        chunk()                               { return m_chunk; }

  uint32_t            size() const                          { return m_chunk.size(); }
  uint32_t            position() const                      { return m_position; }

  uint32_t            incore_length(uint32_t pos);

private:
  mapped_type         m_mapped;

  MemoryChunk         m_chunk;
  uint32_t            m_position;
};

}

#endif
