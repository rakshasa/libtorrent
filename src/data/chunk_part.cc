// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <algorithm>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "chunk_part.h"

namespace torrent {

void
ChunkPart::clear() {
  switch (m_mapped) {
  case MAPPED_MMAP:
    m_chunk.unmap();
    break;

  default:
  case MAPPED_STATIC:
    throw internal_error("ChunkPart::clear() only MAPPED_MMAP supported.");
    break;
  }

  m_chunk.clear();
}

uint32_t
ChunkPart::incore_length(uint32_t pos) {
  // Do we want to use this?
  pos -= m_position;

  if (pos >= size())
    throw internal_error("ChunkPart::incore_length(...) got invalid position");

  int length = size() - pos;
  int touched = m_chunk.pages_touched(pos, length);
  char buf[touched];

  m_chunk.incore(buf, pos, length);

  int dist = std::distance(buf, std::find(buf, buf + touched, 0));

  return std::min(dist ? (dist * m_chunk.page_size() - m_chunk.page_align()) : 0,
                  size() - pos);
}

}
