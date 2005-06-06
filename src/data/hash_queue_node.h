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

#ifndef LIBTORRENT_DATA_HASH_QUEUE_NODE_H
#define LIBTORRENT_DATA_HASH_QUEUE_NODE_H

#include <string>
#include <sigc++/signal.h>

#include "hash_chunk.h"
#include "utils/ref_anchored.h"

namespace torrent {

class HashQueueNode {
public:
  // SignalDone: (Return void)
  // unsigned int - index of chunk
  // std::string  - chunk hash

  typedef RefAnchored<StorageChunk>             Chunk;
  typedef sigc::slot2<void, Chunk, std::string> SlotDone;

  HashQueueNode(HashChunk* c, const std::string& i, SlotDone d) :
    m_chunk(c), m_id(i), m_willneed(false), m_slotDone(d) {}

  uint32_t            get_index() const             { return m_chunk->get_chunk()->get_index(); }
  const std::string&  get_id() const                { return m_id; }

  HashChunk*          get_chunk()                   { return m_chunk; }
  bool                get_willneed() const          { return m_willneed; }

  inline void         clear();

  bool                perform(bool force);

  // Does not call multiple times on the same chunk. Returns the
  // number of bytes not checked in this chunk.
  uint32_t            call_willneed();

  SlotDone&           slot_done()                   { return m_slotDone; }

  bool operator == (const std::string& id) const    { return m_id == id; }

private:
  HashChunk*          m_chunk;
  std::string         m_id;
  bool                m_willneed;

  SlotDone            m_slotDone;
};

inline void
HashQueueNode::clear() {
  delete m_chunk;
  m_chunk = NULL;
}

}

#endif
