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
#include "hash_chunk.h"
#include "storage_chunk.h"

namespace torrent {

bool HashChunk::perform(uint32_t length, bool force) {
  if (!m_chunk.is_valid() || !m_chunk->is_valid())
    throw internal_error("HashChunk::perform(...) called on an invalid chunk");

  length = std::min(length, remaining());

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::perhform(...) received length out of range");
  
  // TODO: Length should really only be an advise, we can optimize here IMO.

  while (length) {
    StorageChunk::Node& node = m_chunk->get_position(m_position);

    uint32_t l = std::min(length, node.length - (m_position - node.position));

    // Usually we only have one or two files in a chunk, so we don't mind the if statement
    // inside the loop.

    if (force) {
      m_hash.update(node.chunk.begin() + m_position - node.position, l);

      length     -= l;
      m_position += l;

    } else {
      uint32_t incore, size = node.chunk.page_touched(m_position - node.position, l);
      unsigned char buf[size];

      // TODO: We're borking here with NULL node.
      node.chunk.incore(buf, m_position - node.position, l);

      for (incore = 0; incore < size; ++incore)
	if (!buf[incore])
	  break;

      if (incore == 0)
	return false;

      l = std::min(l, incore * node.chunk.page_size() - node.chunk.page_align(m_position - node.position));

      m_hash.update(node.chunk.begin() + m_position - node.position, l);

      length     -= l;
      m_position += l;
      
      if (incore < size)
	return false;
    }
  }

  return true;
}

// Warning: Can paralyze linux 2.4.20.
bool HashChunk::willneed(uint32_t length) {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::willneed(...) called on an invalid chunk");

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::willneed(...) received length out of range");

  uint32_t pos = m_position;

  while (length) {
    StorageChunk::Node& node = m_chunk->get_position(pos);

    uint32_t l = std::min(length, node.length - (pos - node.position));

    node.chunk.advise(pos - node.position, l, FileChunk::advice_willneed);

    pos    += l;
    length -= l;
  }

  return true;
}

uint32_t HashChunk::remaining() {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::remaining() called on an invalid chunk");

  return m_chunk->get_size() - m_position;
}

uint32_t HashChunk::remaining_file() {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::remaining_chunk() called on an invalid chunk");
  
  if (m_position == m_chunk->get_size())
    return 0;

  StorageChunk::Node& node = m_chunk->get_position(m_position);

  return node.length - (m_position - node.position);
}    

}
