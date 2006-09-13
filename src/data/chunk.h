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

#ifndef LIBTORRENT_STORAGE_CHUNK_H
#define LIBTORRENT_STORAGE_CHUNK_H

#include <vector>
#include "chunk_part.h"

namespace torrent {

class Chunk : private std::vector<ChunkPart> {
public:
  typedef std::vector<ChunkPart>    base_type;
  typedef std::pair<void*,uint32_t> data_type;

  using base_type::value_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::empty;
  using base_type::reserve;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  Chunk() : m_chunkSize(0), m_prot(~0) {}
  ~Chunk() { clear(); }

  bool                is_all_valid() const;

  // All permissions are set for empty chunks.
  bool                is_readable() const             { return m_prot & MemoryChunk::prot_read; }
  bool                is_writable() const             { return m_prot & MemoryChunk::prot_write; }
  bool                has_permissions(int prot) const { return !(prot & ~m_prot); }

  uint32_t            chunk_size() const              { return m_chunkSize; }

  void                clear();

  void                push_back(value_type::mapped_type mapped, const MemoryChunk& c);

  iterator            at_position(uint32_t pos);
  iterator            at_position(uint32_t pos, iterator itr);

  data_type           at_memory(uint32_t offset, iterator part);

  // Check how much of the chunk is incore from pos.
  uint32_t            incore_length(uint32_t pos);

  bool                sync(int flags);

  void                preload(uint32_t position, uint32_t length);

  bool                to_buffer(void* buffer, uint32_t position, uint32_t length);
  bool                from_buffer(const void* buffer, uint32_t position, uint32_t length);
  bool                compare_buffer(const void* buffer, uint32_t position, uint32_t length);

private:
  Chunk(const Chunk&);
  void operator = (const Chunk&);
  
  uint32_t            m_chunkSize;
  int                 m_prot;
};

inline Chunk::iterator
Chunk::at_position(uint32_t pos, iterator itr) {
  while (itr != end() && itr->position() + itr->size() <= pos)
    itr++;

  return itr;
}

}

#endif

  
