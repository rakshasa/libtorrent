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

#ifndef LIBTORRENT_DATA_STORAGE_NODE_H
#define LIBTORRENT_DATA_STORAGE_NODE_H

#include "memory_chunk.h"

namespace torrent {

class StorageNode {
public:
  StorageNode(const MemoryChunk& c, uint32_t pos, uint32_t len) : m_chunk(c), m_position(pos), m_length(len) {}

  bool                is_valid() const                      { return m_chunk.is_valid(); }
  bool                is_contained(uint32_t p) const        { return p >= m_position && p < m_position + m_length; }

  void                clear();

  MemoryChunk&        get_chunk()                           { return m_chunk; }
  uint32_t            get_position() const                  { return m_position; }
  uint32_t            get_length() const                    { return m_length; }

private:
  MemoryChunk         m_chunk;

  uint32_t            m_position;
  uint32_t            m_length;
};

}

#endif
