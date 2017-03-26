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

#ifndef LIBTORRENT_HASH_CHUNK_H
#define LIBTORRENT_HASH_CHUNK_H

#include "torrent/exceptions.h"
#include "utils/sha1.h"

#include "chunk.h"
#include "chunk_handle.h"

namespace torrent {

// This class interface assumes we're always going to check the whole
// chunk. All we need is control of the (non-)blocking nature, and other
// stuff related to performance and responsiveness.

class ChunkListNode;

class HashChunk {
public:
  HashChunk()         {}
  HashChunk(ChunkHandle h)  { set_chunk(h); }

  void                set_chunk(ChunkHandle h)                { m_position = 0; m_chunk = h; m_hash.init(); }

  ChunkHandle*        chunk()                                 { return &m_chunk; }
  ChunkHandle&        handle()                                { return m_chunk; }
  void                hash_c(char* buffer)                    { m_hash.final_c(buffer); }

  // If force is true, then the return value is always true.
  bool                perform(uint32_t length, bool force = true);

  void                advise_willneed(uint32_t length);

  uint32_t            remaining();

private:
  inline uint32_t     remaining_part(Chunk::iterator itr, uint32_t pos);
  uint32_t            perform_part(Chunk::iterator itr, uint32_t length);

  uint32_t            m_position;

  ChunkHandle         m_chunk;
  Sha1                m_hash;
};

inline uint32_t
HashChunk::remaining_part(Chunk::iterator itr, uint32_t pos) {
  return itr->size() - (pos - itr->position());
}

inline uint32_t
HashChunk::remaining() {
  if (!m_chunk.is_loaded())
    throw internal_error("HashChunk::remaining(...) called on an invalid chunk");

  return m_chunk.chunk()->chunk_size() - m_position;
}

}

#endif
