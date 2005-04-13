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

#ifndef LIBTORRENT_HASH_CHUNK_H
#define LIBTORRENT_HASH_CHUNK_H

#include <algo/ref_anchored.h>

#include "utils/sha1.h"
#include "storage_chunk.h"

namespace torrent {

// This class interface assumes we're always going to check the whole
// chunk. All we need is control of the (non-)blocking nature, and other
// stuff related to performance and responsiveness.

class HashChunk {
public:
  typedef algo::RefAnchored<StorageChunk> Chunk;

  HashChunk()         {}
  HashChunk(Chunk c)  { set_chunk(c); }
  
  void                set_chunk(Chunk c)                      { m_position = 0; m_chunk = c; m_hash.init(); }

  Chunk               get_chunk()                             { return m_chunk; }
  std::string         get_hash()                              { return m_hash.final(); }

  // If force is true, then the return value is always true.
  bool                perform(uint32_t length, bool force = true);

  void                advise_willneed(uint32_t length);

  uint32_t            remaining();

private:
  inline uint32_t     remaining_part(StorageChunk::iterator itr, uint32_t pos);
  uint32_t            perform_part(StorageChunk::iterator itr, uint32_t length);

  uint32_t            m_position;

  Chunk               m_chunk;
  Sha1                m_hash;
};

inline uint32_t
HashChunk::remaining_part(StorageChunk::iterator itr, uint32_t pos) {
  return itr->size() - (pos - itr->get_position());
}

}

#endif

