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

Storage::Storage() :
  m_consolidator(new StorageConsolidator()) {
}

Storage::~Storage() {
  close();

  delete m_consolidator;
}

void
Storage::set_chunksize(uint32_t s) {
  m_consolidator->set_chunksize(s);

  m_anchors.resize(get_chunk_total());
}

Storage::Chunk
Storage::get_chunk(unsigned int b, bool wr, bool rd) {
  if (b >= m_anchors.size())
    throw internal_error("Chunk out of range in Storage");

  if (m_anchors[b].is_valid())
    return Chunk(m_anchors[b]);

  Chunk chunk(new StorageChunk(b));

  if (!m_consolidator->get_chunk(*chunk, b, wr, rd))
    return Chunk();
  
  chunk.anchor(m_anchors[b]);

  return chunk;
}

Storage::FileList& 
Storage::get_files() {
  return m_consolidator->get_files();
}

}
