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

#ifndef LIBTORRENT_DATA_HASH_QUEUE_NODE_H
#define LIBTORRENT_DATA_HASH_QUEUE_NODE_H

#include <cinttypes>
#include <functional>
#include <string>

#include "chunk_handle.h"
#include "hash_chunk.h"

namespace torrent {

class download_data;

class HashQueueNode {
public:
  typedef std::function<void (ChunkHandle, const char*)> slot_done_type;
  typedef download_data* id_type;

  HashQueueNode(id_type id, HashChunk* c, slot_done_type d) :
    m_id(id), m_chunk(c), m_willneed(false), m_slot_done(d) {}

  id_type             id() const                    { return m_id; }
  ChunkHandle&        handle()                      { return *m_chunk->chunk(); }

  uint32_t            get_index() const;

  HashChunk*          get_chunk()                   { return m_chunk; }
  bool                get_willneed() const          { return m_willneed; }

  bool                perform_remaining(bool force) { return m_chunk->perform(m_chunk->remaining(), force); }

  void                clear();

  // Does not call multiple times on the same chunk. Returns the
  // number of bytes not checked in this chunk.
  uint32_t            call_willneed();

  slot_done_type&     slot_done()                   { return m_slot_done; }

private:
  id_type             m_id;
  HashChunk*          m_chunk;
  bool                m_willneed;

  slot_done_type      m_slot_done;
};

}

#endif
