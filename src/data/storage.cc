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

#include "storage.h"

namespace torrent {

void
Storage::set_chunk_size(uint32_t s) {
  m_consolidator.set_chunk_size(s);

  m_anchors.resize(get_chunk_total());
}

Storage::Chunk
Storage::get_chunk(uint32_t b, int prot) {
  if (b >= m_anchors.size())
    throw internal_error("Chunk out of range in Storage");

  if (m_anchors[b].is_valid() &&
      m_anchors[b].data()->has_permissions(prot))
    return Chunk(m_anchors[b]);

  // TODO: We will propably need to store a per StorageChunk prot.
//   if (m_anchors[b].is_valid())
//     prot |= m_anchors[b].data()->get_prot();

  Chunk chunk(new StorageChunk(b));

  if (!m_consolidator.get_chunk(*chunk, b, prot))
    return Chunk();
  
  chunk.anchor(m_anchors[b]);

  return chunk;
}

}
