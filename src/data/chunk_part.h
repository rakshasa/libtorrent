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

#ifndef LIBTORRENT_DATA_STORAGE_CHUNK_PART_H
#define LIBTORRENT_DATA_STORAGE_CHUNK_PART_H

#include "memory_chunk.h"

namespace torrent {

class File;

class lt_cacheline_aligned ChunkPart {
public:
  typedef enum {
    MAPPED_MMAP,
    MAPPED_STATIC
  } mapped_type;

  ChunkPart(mapped_type mapped, const MemoryChunk& c, uint32_t pos) :
    m_mapped(mapped), m_chunk(c), m_position(pos), m_file(NULL), m_file_offset(0) {}

  bool                is_valid() const                      { return m_chunk.is_valid(); }
  bool                is_contained(uint32_t p) const        { return p >= m_position && p < m_position + size(); }

  bool                has_address(void* p) const            { return (char*)p >= m_chunk.begin() && p < m_chunk.end(); }

  void                clear();

  mapped_type         mapped() const                        { return m_mapped; }

  MemoryChunk&        chunk()                               { return m_chunk; }
  const MemoryChunk&  chunk() const                         { return m_chunk; }

  uint32_t            size() const                          { return m_chunk.size(); }
  uint32_t            position() const                      { return m_position; }

  uint32_t            remaining_from(uint32_t pos) const    { return size() - (pos - m_position); }

  File*               file() const                          { return m_file; }
  uint64_t            file_offset() const                   { return m_file_offset; }

  void                set_file(File* f, uint64_t f_offset)  { m_file = f; m_file_offset = f_offset; }

  bool                is_incore(uint32_t pos, uint32_t length = ~uint32_t());
  uint32_t            incore_length(uint32_t pos, uint32_t length = ~uint32_t());

private:
  mapped_type         m_mapped;

  MemoryChunk         m_chunk;
  uint32_t            m_position;

  // Currently used to figure out what file and where a SIGBUS
  // occurred. Can also be used in the future to indicate if a part is
  // temporary storage, etc.
  File*               m_file;
  uint64_t            m_file_offset;
};

}

#endif
