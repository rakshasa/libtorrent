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

#ifndef LIBTORRENT_HASH_QUEUE_H
#define LIBTORRENT_HASH_QUEUE_H

#include <string>
#include <sigc++/signal.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"

#include "utils/task.h"

namespace torrent {

class HashChunk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class HashQueue {
public:
  // SignalDone: (Return void)
  // unsigned int - index of chunk
  // std::string  - chunk hash

  typedef algo::RefAnchored<StorageChunk>       Chunk;
  typedef sigc::slot2<void, Chunk, std::string> SlotDone;

  HashQueue();
  ~HashQueue() { clear(); }

  void                add(Chunk c, SlotDone d, const std::string& id);

  bool                has(uint32_t index, const std::string& id);

  void                remove(const std::string& id);
  void                clear();

  void                work();

private:
  struct Node {
    Node(HashChunk* c, const std::string& i, SlotDone d) :
      m_chunk(c), m_id(i), m_done(d), m_willneed(false) {}

    uint32_t          get_index();

    HashChunk*        m_chunk;
    std::string       m_id;
    SlotDone          m_done;

    bool              m_willneed;
  };

  bool                check(bool force);

  void                willneed(int count);

  typedef std::list<Node> ChunkList;

  uint16_t            m_tries;
  ChunkList           m_chunks;

  Task                m_taskWork;
};

}

#endif
