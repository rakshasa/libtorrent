// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#ifndef LIBTORRENT_DATA_CHUNK_ITERATOR_H
#define LIBTORRENT_DATA_CHUNK_ITERATOR_H

#include "chunk.h"

namespace torrent {

class ChunkIterator {
public:
  ChunkIterator(Chunk* chunk, uint32_t first, uint32_t last);
  
  bool                empty() const { return m_iterator == m_chunk->end() || m_first >= m_last; }

  // Only non-zero length ranges will be returned.
  Chunk::data_type    data();

  MemoryChunk*        memory_chunk() { return &m_iterator->chunk(); }

  uint32_t            memory_chunk_first() const { return m_first - m_iterator->position(); }
  uint32_t            memory_chunk_last() const { return m_last - m_iterator->position(); }

  bool                next();
  bool                forward(uint32_t length);

private:
  Chunk*              m_chunk;
  Chunk::iterator     m_iterator;
  
  uint32_t            m_first;
  uint32_t            m_last;
};

inline
ChunkIterator::ChunkIterator(Chunk* chunk, uint32_t first, uint32_t last) :
  m_chunk(chunk),
  m_iterator(chunk->at_position(first)),

  m_first(first),
  m_last(last) {
}

inline Chunk::data_type
ChunkIterator::data() {
  Chunk::data_type data = m_chunk->at_memory(m_first, m_iterator);
  data.second = std::min(data.second, m_last - m_first);

  return data;
}

inline bool
ChunkIterator::next() {
  m_first = m_iterator->position() + m_iterator->size();

  while (++m_iterator != m_chunk->end()) {
    if (m_iterator->size() != 0)
      return m_first < m_last;
  }

  return false;
}

// Returns true if the new position is on a file boundary while not at
// the edges of the chunk.
//
// Do not return true if the length was zero, in order to avoid
// getting stuck looping when no data is being read/written.
inline bool
ChunkIterator::forward(uint32_t length) {
  m_first += length;

  if (m_first >= m_last)
    return false;

  do {
    if (m_first < m_iterator->position() + m_iterator->size())
      return true;

    m_iterator++;
  } while (m_iterator != m_chunk->end());

  return false;
}

} // namespace torrent

#endif
