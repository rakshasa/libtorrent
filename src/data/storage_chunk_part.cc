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

#include <algorithm>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "storage_chunk_part.h"

namespace torrent {

void
StorageChunkPart::clear() {
  m_chunk.unmap();
  m_chunk.clear();
}

uint32_t
StorageChunkPart::incore_length(uint32_t pos) {
  // Do we want to use this?
  pos -= m_position;

  if (pos >= size())
    throw internal_error("StorageChunkPart::incore_length(...) got invalid position");

  int length = size() - pos;
  int touched = m_chunk.pages_touched(pos, length);
  char buf[touched];

  m_chunk.incore(buf, pos, length);

  int dist = std::distance(buf, std::find(buf, buf + touched, 0));

  return std::min(dist ? (dist * m_chunk.page_size() - m_chunk.page_align()) : 0,
		  size() - pos);
}

}
