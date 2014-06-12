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

#include "data/hash_chunk.h"
#include "torrent/hash_string.h"
#include "utils/instrumentation.h"

namespace torrent {

HashCheckQueue::HashCheckQueue() {
  pthread_mutex_init(&m_lock, NULL);
}

HashCheckQueue::~HashCheckQueue() {
  pthread_mutex_destroy(&m_lock);
}

// Always poke thread_disk after calling this.
void
HashCheckQueue::push_back(HashChunk* hash_chunk) {
  if (hash_chunk == NULL || !hash_chunk->chunk()->is_loaded() || !hash_chunk->chunk()->is_blocking())
    throw internal_error("Invalid hash chunk passed to HashCheckQueue.");

  pthread_mutex_lock(&m_lock);

  // Set blocking...(? this needs to be possible to do after getting
  // the chunk) When doing this make sure we verify that the handle is
  // not previously blocked.

  base_type::push_back(hash_chunk);

  int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
  instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, 1);
  instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, size);

  pthread_mutex_unlock(&m_lock);
}

// erase...
//
// The erasing function should call slot, perhaps return a bool if we
// deleted, or in the rare case it has already been hashed and will
// arraive in the near future?
//
// We could handle this by polling the done chunks on false return
// values.

// Lock the chunk list and increment blocking only when starting the
// checking.

// No removal of entries, only clearing.

bool
HashCheckQueue::remove(HashChunk* hash_chunk) {
  pthread_mutex_lock(&m_lock);

  bool result;
  iterator itr = std::find(begin(), end(), hash_chunk);

  if (itr != end()) {
    base_type::erase(itr);
    result = true;

    int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, -1);
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, -size);

  } else {
    result = false;
  }

  pthread_mutex_unlock(&m_lock);
  return result;
}

void
HashCheckQueue::perform() {
  pthread_mutex_lock(&m_lock);

  while (!empty()) {
    HashChunk* hash_chunk = base_type::front();
    base_type::pop_front();
    
    if (!hash_chunk->chunk()->is_loaded())
      throw internal_error("HashCheckQueue::perform(): !entry.node->is_loaded().");

    int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, -1);
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, -size);

    pthread_mutex_unlock(&m_lock);

    if (!hash_chunk->perform(~uint32_t(), true))
      throw internal_error("HashCheckQueue::perform(): !hash_chunk->perform(~uint32_t(), true).");

    HashString hash;
    hash_chunk->hash_c(hash.data());

    m_slot_chunk_done(hash_chunk, hash);
    pthread_mutex_lock(&m_lock);
  }

  pthread_mutex_unlock(&m_lock);
}

}
