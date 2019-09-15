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

#include <functional>
#include <rak/functional.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/data/download_data.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread_base.h"

#include "hash_queue.h"
#include "hash_chunk.h"
#include "chunk.h"
#include "chunk_list_node.h"
#include "globals.h"
#include "thread_disk.h"

#define LT_LOG_DATA(data, log_level, log_fmt, ...)                       \
  lt_log_print_data(LOG_STORAGE_##log_level, data, "hash_queue", log_fmt, __VA_ARGS__);

namespace torrent {

struct HashQueueEqual {
  HashQueueEqual(HashQueueNode::id_type id, uint32_t index) : m_id(id), m_index(index) {}

  bool operator () (const HashQueueNode& q) const { return m_id == q.id() && m_index == q.get_index(); }

  HashQueueNode::id_type m_id;
  uint32_t              m_index;
};

struct HashQueueWillneed {
  HashQueueWillneed(int bytes) : m_bytes(bytes) {}

  bool operator () (HashQueueNode& q) { return (m_bytes -= q.call_willneed()) <= 0; }

  int m_bytes;
};

// If madvise is not available it will always mark the pages as being
// in memory, thus we don't need to modify m_maxTries to have full
// disk usage. But this may cause too much blocking as it will think
// everything is in memory, thus we need to throttle.

HashQueue::HashQueue(thread_disk* thread) :
  m_thread_disk(thread) {

  pthread_mutex_init(&m_done_chunks_lock, NULL);
  m_thread_disk->hash_queue()->slot_chunk_done() = std::bind(&HashQueue::chunk_done, this, std::placeholders::_1, std::placeholders::_2);
}


// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
void
HashQueue::push_back(ChunkHandle handle, HashQueueNode::id_type id, slot_done_type d) {
  LT_LOG_DATA(id, DEBUG, "Adding index:%" PRIu32 " to queue.", handle.index());

  if (!handle.is_loaded())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  HashChunk* hash_chunk = new HashChunk(handle);

  base_type::push_back(HashQueueNode(id, hash_chunk, d));

  m_thread_disk->hash_queue()->push_back(hash_chunk);
  m_thread_disk->interrupt();
}

bool
HashQueue::has(HashQueueNode::id_type id) {
  return std::find_if(begin(), end(), rak::equal(id, std::mem_fun_ref(&HashQueueNode::id))) != end();
}

bool
HashQueue::has(HashQueueNode::id_type id, uint32_t index) {
  return std::find_if(begin(), end(), HashQueueEqual(id, index)) != end();
}

void
HashQueue::remove(HashQueueNode::id_type id) {
  iterator itr = begin();
  
  while ((itr = std::find_if(itr, end(), rak::equal(id, std::mem_fun_ref(&HashQueueNode::id)))) != end()) {
    HashChunk *hash_chunk = itr->get_chunk();

    LT_LOG_DATA(id, DEBUG, "Removing index:%" PRIu32 " from queue.", hash_chunk->handle().index());

    thread_base::release_global_lock();
    bool result = m_thread_disk->hash_queue()->remove(hash_chunk);
    thread_base::acquire_global_lock();

    // The hash chunk was not found, so we need to wait until the hash
    // check finishes.
    if (!result) {
      pthread_mutex_lock(&m_done_chunks_lock);
      done_chunks_type::iterator done_itr;

      while ((done_itr = m_done_chunks.find(hash_chunk)) == m_done_chunks.end()) {
        pthread_mutex_unlock(&m_done_chunks_lock);
        usleep(100);
        pthread_mutex_lock(&m_done_chunks_lock);
      }

      m_done_chunks.erase(done_itr);
      pthread_mutex_unlock(&m_done_chunks_lock);
    }

    itr->slot_done()(*hash_chunk->chunk(), NULL);
    itr->clear();

    itr = erase(itr);
  }
}

void
HashQueue::clear() {
  if (!empty())
    throw internal_error("HashQueue::clear() called but valid nodes were found.");

  // Replace with a dtor check to ensure it is empty?
//   std::for_each(begin(), end(), std::mem_fun_ref(&HashQueueNode::clear));
//   base_type::clear();
}

void
HashQueue::work() {
  pthread_mutex_lock(&m_done_chunks_lock);
    
  while (!m_done_chunks.empty()) {
    HashChunk* hash_chunk = m_done_chunks.begin()->first;
    HashString hash_value = m_done_chunks.begin()->second;
    m_done_chunks.erase(m_done_chunks.begin());

    // TODO: This is not optimal as we jump around... Check for front
    // of HashQueue in done_chunks instead.

    iterator itr = std::find_if(begin(), end(), std::bind(std::equal_to<HashChunk*>(),
                                                          hash_chunk,
                                                          std::bind(&HashQueueNode::get_chunk, std::placeholders::_1)));

    // TODO: Fix this...
    if (itr == end())
      throw internal_error("Could not find done chunk's node.");

    LT_LOG_DATA(itr->id(), DEBUG, "Passing index:%" PRIu32 " to owner: %s.",
                hash_chunk->handle().index(),
                hash_string_to_hex_str(hash_value).c_str());

    HashQueueNode::slot_done_type slotDone = itr->slot_done();
    base_type::erase(itr);

    slotDone(hash_chunk->handle(), hash_value.c_str());
    delete hash_chunk;
  }

  pthread_mutex_unlock(&m_done_chunks_lock);
}

void
HashQueue::chunk_done(HashChunk* hash_chunk, const HashString& hash_value) {
  pthread_mutex_lock(&m_done_chunks_lock);

  m_done_chunks[hash_chunk] = hash_value;
  m_slot_has_work(m_done_chunks.empty());

  pthread_mutex_unlock(&m_done_chunks_lock);
}

}
