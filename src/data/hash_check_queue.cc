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

#include "config.h"

#include "hash_check_queue.h"

#include "data/hash_queue_node.h"
#include "torrent/hash_string.h"

namespace torrent {

void
HashCheckQueue::push_back(const ChunkHandle& handle, HashQueueNode* node) {
  pthread_mutex_lock(&m_lock);

  // Set blocking...(? this needs to be possible to do after getting
  // the chunk) When doing this make sure we verify that the handle is
  // not previously blocked.

  hash_check_queue_node entry = { handle, node };
  base_type::push_back(entry);

  // TODO: Poke thread.

  pthread_mutex_unlock(&m_lock);
}

// Lock the chunk list and increment blocking only when starting the
// checking.

// No removal of entries, only clearing.

void
HashCheckQueue::perform() {
  // Use an atomic/safer way of checking?
  // if (empty())
  //   return;

  pthread_mutex_lock(&m_lock);

  while (!empty()) {
    hash_check_queue_node entry = base_type::front();
    base_type::pop_front();
    
    if (!entry.handle.is_valid())
      throw internal_error("HashCheckQueue::perform(): !entry.node->is_valid().");

    pthread_mutex_unlock(&m_lock);

    // Use handle or node?
    HashChunk hash_chunk(entry.handle);

    if (!hash_chunk.perform(~uint32_t(), true))
      throw internal_error("HashCheckQueue::perform(): !hash_chunk.perform(~uint32_t(), true).");

    HashString hash;
    hash_chunk.hash_c(hash.data());

    pthread_mutex_lock(&m_lock);

    // Call slot. (TODO: Needs to grab global lock?)
    m_slot_chunk_done(entry.handle, entry.node, hash);

    // Free the blocking state once done.
    // delete chunk;
  }

  pthread_mutex_unlock(&m_lock);
}

}
