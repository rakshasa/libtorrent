// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "torrent/exceptions.h"

#include "chunk_list.h"
#include "chunk.h"
#include "globals.h"

namespace torrent {

inline bool
ChunkList::is_queued(ChunkListNode* node) {
  return std::find(m_queue.begin(), m_queue.end(), node) != m_queue.end();
}

bool
ChunkList::has_chunk(size_type index, int prot) const {
  return
    Base::at(index).is_valid() &&
    Base::at(index).chunk()->has_permissions(prot);
}

void
ChunkList::resize(size_type s) {
  if (!empty())
    throw internal_error("ChunkList::resize(...) called on an non-empty object.");

  Base::resize(s);

  uint32_t index = 0;

  for (iterator itr = begin(), last = end(); itr != last; ++itr, ++index)
    itr->set_index(index);
}

void
ChunkList::clear() {
  if (!m_queue.empty())
    throw internal_error("ChunkList::clear() m_queue could not be clear.");

  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::references)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::writable)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  Base::clear();
}

ChunkHandle
ChunkList::get(size_type index, bool writable) {
  ChunkListNode* node = &Base::at(index);

  if (!node->is_valid() || (writable && !node->chunk()->is_writable())) {
    CreateChunk chunk = m_slotCreateChunk(index, writable);

    if (chunk.first == NULL) {
      ChunkHandle c;
      c.set_error_number(chunk.second);
      
      return c;
    }

    if (node->is_valid())
      delete node->chunk();

    node->set_chunk(chunk.first);
    node->set_time_modified(rak::timer());
  }

  node->inc_references();

  if (writable)
    node->inc_writable();

  return ChunkHandle(node, writable);
}

// The chunks in 'm_queue' have been modified and need to be synced
// when appropriate. Hopefully keeping the chunks mmap'ed for a while
// will allow us to schedule writes at more resonable intervals.

void
ChunkList::release(ChunkHandle handle) {
  if (!handle.is_valid())
    throw internal_error("ChunkList::release(...) received an invalid handle.");

  if (handle.node() < &*begin() || handle.node() >= &*end())
    throw internal_error("ChunkList::release(...) received an unknown handle.");

  if (handle->references() <= 0 || (handle.is_writable() && handle->writable() <= 0))
    throw internal_error("ChunkList::release(...) received a node with bad reference count.");

  if (handle.is_writable()) {

    if (handle->writable() == 1) {
      if (is_queued(handle.node()))
	throw internal_error("ChunkList::release(...) tried to queue an already queued chunk.");

      // Only add those that have a modification time set?
      m_queue.push_back(handle.node());

    } else {
      handle->dec_references();
      handle->dec_writable();
    }

  } else {
    if (handle->references() == 1) {
      if (is_queued(handle.node()))
	throw internal_error("ChunkList::release(...) tried to unmap a queued chunk.");

      delete handle->chunk();
      handle->set_chunk(NULL);
      
    }

    handle->dec_references();
  }
}

struct chunk_list_last_modified {
  chunk_list_last_modified() : m_time(cachedTime) {}

  void operator () (ChunkListNode* node) {
    if (node->time_modified() < m_time && node->time_modified() != rak::timer())
      m_time = node->time_modified();
  }

  rak::timer m_time;
};

inline bool
ChunkList::sync_chunk(ChunkListNode* node) {
  if (node->references() <= 0 || node->writable() <= 0)
    throw internal_error("ChunkList::sync_chunk(...) got a node with invalid reference count.");

  // Using sync for the time being.
  if (!node->chunk()->sync(MemoryChunk::sync_sync))
    return false;

  node->dec_writable();
  node->dec_references();
 
  if (node->references() == 0) {
    delete node->chunk();
    node->set_chunk(NULL);
  }

  return true;
}

inline bool
ChunkList::less_chunk_index(ChunkListNode* node1, ChunkListNode* node2) {
  return node1->index() < node2->index();
}

unsigned int
ChunkList::sync_all() {
//   Queue::iterator split = std::stable_partition(m_queue.begin(), m_queue.end(), rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));
  Queue::iterator split = m_queue.begin();
  unsigned int failed = 0;

  for (Queue::iterator itr = split, last = m_queue.end(); itr != last; ++itr) {

    // Do first async, then sync at the next call.

    if (!sync_chunk(*itr)) {
      // Makes sure the failed chunks don't get erased.
      std::iter_swap(itr, split++);
      failed++;
    }
  }

  m_queue.erase(split, m_queue.end());
  return failed;
}

unsigned int
ChunkList::sync_periodic() {
  if (std::find_if(m_queue.begin(), m_queue.end(), rak::equal(0, std::mem_fun(&ChunkListNode::writable)))
      != m_queue.end())
    throw internal_error("ChunkList::sync_periodic() found a chunk with writable == 0.");

  // Chunks that still have writers are moved infront of split. Those
  // chunks were once released by the last writer, and later grabbed
  // again. This should be relatively rare and should not cause any
  // problems.
  //
  // Using std::stable_partition to keep the index order relatively
  // intact for quicker sorting.
  Queue::iterator split = std::stable_partition(m_queue.begin(), m_queue.end(), rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));

  // Do various partitioning before sorting by index, so that when we
  // call sync we always increase the index of the chunk.
  //
  // Some times when we sync there are users holding write references
  // to chunks in the queue. These won't explicitly be synced, but the
  // kernel might do so anyway if it lies in its path, so we don't
  // sync those chunks.

  if (std::distance(split, m_queue.end()) < (difference_type)m_maxQueueSize &&
      std::for_each(split, m_queue.end(), chunk_list_last_modified()).m_time + rak::timer::from_seconds(m_maxTimeQueued) < cachedTime)
    return 0;

  std::sort(split, m_queue.end(), std::ptr_fun(&ChunkList::less_chunk_index));

  unsigned int failed = 0;

  for (Queue::iterator itr = split, last = m_queue.end(); itr != last; ++itr) {

    // Do first async, then sync at the next call.

    if (!sync_chunk(*itr)) {
      // Makes sure the failed chunks don't get erased.
      std::iter_swap(itr, split++);
      failed++;
    }
  }

  m_queue.erase(split, m_queue.end());
  return failed;
}

}
