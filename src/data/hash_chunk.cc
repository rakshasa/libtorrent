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

bool
HashChunk::perform(uint32_t length, bool force) {
  if (!m_chunk.is_valid() || !m_chunk->is_valid())
    throw internal_error("HashChunk::perform(...) called on an invalid chunk");

  length = std::min(length, remaining());

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::perhform(...) received length out of range");
  
  uint32_t l = force ? length : m_chunk->incore_length(m_position);

  bool complete = l == length;

  while (l) {
    StorageChunk::iterator node = m_chunk->at_position(m_position);

    l -= perform_part(node, l);
  }

  return complete;
}

void
HashChunk::advise_willneed(uint32_t length) {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::willneed(...) called on an invalid chunk");

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::willneed(...) received length out of range");

  uint32_t pos = m_position;

  while (length) {
    StorageChunk::iterator node = m_chunk->at_position(pos);

    uint32_t l = std::min(length, remaining_part(node, pos));

    node->get_chunk().advise(pos - node->get_position(), l, MemoryChunk::advice_willneed);

    pos    += l;
    length -= l;
  }
}

uint32_t
HashChunk::remaining() {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::remaining() called on an invalid chunk");

  return m_chunk->get_size() - m_position;
}

uint32_t
HashChunk::perform_part(StorageChunk::iterator itr, uint32_t length) {
  length = std::min(length, remaining_part(itr, m_position));
  
  m_hash.update(itr->get_chunk().begin() + m_position - itr->get_position(), length);
  m_position += length;

  return length;
}

}
