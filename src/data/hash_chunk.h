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

#include "utils/sha1.h"
#include "storage_chunk.h"

namespace torrent {

// This class interface assumes we're always going to check the whole
// chunk. All we need is control of the (non-)blocking nature, and other
// stuff related to performance and responsiveness.

class ChunkListNode;

class HashChunk {
public:
  HashChunk()         {}
  HashChunk(ChunkListNode* c)  { set_chunk(c); }
  
  void                set_chunk(ChunkListNode* c)             { m_position = 0; m_chunk = c; m_hash.init(); }

  ChunkListNode*      get_chunk()                             { return m_chunk; }
  std::string         get_hash()                              { return m_hash.final(); }

  // If force is true, then the return value is always true.
  bool                perform(uint32_t length, bool force = true);

  void                advise_willneed(uint32_t length);

  uint32_t            remaining();

private:
  inline uint32_t     remaining_part(StorageChunk::iterator itr, uint32_t pos);
  uint32_t            perform_part(StorageChunk::iterator itr, uint32_t length);

  uint32_t            m_position;

  ChunkListNode*      m_chunk;
  Sha1                m_hash;
};

inline uint32_t
HashChunk::remaining_part(StorageChunk::iterator itr, uint32_t pos) {
  return itr->size() - (pos - itr->get_position());
}

}

#endif

