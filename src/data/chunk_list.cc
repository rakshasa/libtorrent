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
#include "torrent/chunk_manager.h"

#include "chunk_list.h"
#include "chunk.h"
#include "globals.h"

namespace torrent {

struct chunk_list_earliest_modified {
  chunk_list_earliest_modified() : m_time(cachedTime) {}

  void operator () (ChunkListNode* node) {
    if (node->time_modified() < m_time && node->time_modified() != rak::timer())
      m_time = node->time_modified();
  }

  rak::timer m_time;
};

struct chunk_list_sort_index {
  bool operator () (ChunkListNode* node1, ChunkListNode* node2) {
    return node1->index() < node2->index();
  }
};

inline bool
ChunkList::is_queued(ChunkListNode* node) {
  return std::find(m_queue.begin(), m_queue.end(), node) != m_queue.end();
}

bool
ChunkList::has_chunk(size_type index, int prot) const {
  return base_type::at(index).is_valid() && base_type::at(index).chunk()->has_permissions(prot);
}

void
ChunkList::resize(size_type s) {
  if (!empty())
    throw internal_error("ChunkList::resize(...) called on an non-empty object.");

  base_type::resize(s);

  uint32_t index = 0;

  for (iterator itr = begin(), last = end(); itr != last; ++itr, ++index)
    itr->set_index(index);
}

void
ChunkList::clear() {
  // Don't do any sync'ing as whomever decided to shut down really
  // doesn't care, so just de-reference all chunks in queue.
  for (Queue::iterator itr = m_queue.begin(), last = m_queue.end(); itr != last; ++itr) {
    if ((*itr)->references() != 1 || (*itr)->writable() != 1)
      throw internal_error("ChunkList::clear() called but a node in the queue is still referenced.");
    
    (*itr)->dec_rw();
    clear_chunk(*itr);
  }

  m_queue.clear();

  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::chunk)) != end() ||
      std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::references)) != end() ||
      std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::writable)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  base_type::clear();
}

ChunkHandle
ChunkList::get(size_type index, bool writable) {
  ChunkListNode* node = &base_type::at(index);

  if (!node->is_valid()) {
    CreateChunk chunk = m_slotCreateChunk(index, writable);

    if (chunk.first == NULL)
      return ChunkHandle::from_error(chunk.second);

    // Would be cleaner to do this before creating the chunk.
    if (!m_manager->allocate(chunk.first->chunk_size())) {
      delete chunk.first;
      return ChunkHandle::from_error(rak::error_number::e_nomem);
    }

    node->set_chunk(chunk.first);
    node->set_time_modified(rak::timer());

  } else if (writable && !node->chunk()->is_writable()) {
    CreateChunk chunk = m_slotCreateChunk(index, writable);

    if (chunk.first == NULL)
      return ChunkHandle::from_error(chunk.second);

    delete node->chunk();

    node->set_chunk(chunk.first);
    node->set_time_modified(rak::timer());
  }

  node->inc_references();

  if (writable) {
    node->inc_writable();

    // Make sure that periodic syncing uses async on any subsequent
    // changes even if it was triggered before this get.
    node->set_sync_triggered(false);
  }

  return ChunkHandle(node, writable);
}

// The chunks in 'm_queue' have been modified and need to be synced
// when appropriate. Hopefully keeping the chunks mmap'ed for a while
// will allow us to schedule writes at more resonable intervals.

void
ChunkList::release(ChunkHandle* handle) {
  if (!handle->is_valid())
    throw internal_error("ChunkList::release(...) received an invalid handle.");

  if (handle->object() < &*begin() || handle->object() >= &*end())
    throw internal_error("ChunkList::release(...) received an unknown handle.");

  if (handle->object()->references() <= 0 || (handle->is_writable() && handle->object()->writable() <= 0))
    throw internal_error("ChunkList::release(...) received a node with bad reference count.");

  if (handle->is_writable()) {

    if (handle->object()->writable() == 1) {
      if (is_queued(handle->object()))
        throw internal_error("ChunkList::release(...) tried to queue an already queued chunk.");

      // Only add those that have a modification time set?
      //
      // Only chunks that are not already in the queue will execute
      // this branch.
      m_queue.push_back(handle->object());

    } else {
      handle->object()->dec_rw();
    }

  } else {
    handle->object()->dec_references();

    if (handle->object()->references() == 0) {
      if (is_queued(handle->object()))
        throw internal_error("ChunkList::release(...) tried to unmap a queued chunk.");

      clear_chunk(handle->object());
    }
  }

  handle->clear();
}

void
ChunkList::clear_chunk(ChunkListNode* node) {
  if (!node->is_valid())
    throw internal_error("ChunkList::clear_chunk(...) !node->is_valid().");

  uint32_t size = node->chunk()->chunk_size();

  delete node->chunk();
  node->set_chunk(NULL);

  m_manager->deallocate(size);
}

inline bool
ChunkList::sync_chunk(ChunkListNode* node, std::pair<int,bool> options) {
  if (node->references() <= 0 || node->writable() <= 0)
    throw internal_error("ChunkList::sync_chunk(...) got a node with invalid reference count.");

  if (!node->chunk()->sync(options.first))
    return false;

  node->set_sync_triggered(true);

  if (!options.second)
    return true;

  node->dec_rw();
 
  if (node->references() == 0)
    clear_chunk(node);

  return true;
}

uint32_t
ChunkList::sync_chunks(int flags) {
  Queue::iterator split;

  if (flags & sync_all)
    split = m_queue.begin();
  else
    split = std::stable_partition(m_queue.begin(), m_queue.end(), rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));

  // Allow a flag that does more culling, so that we only get large
  // continous sections.
  //
  // How does this interact with timers, should be make it so that
  // only areas with timers are (preferably) synced?

  std::sort(split, m_queue.end());
  
  // If we got enough diskspace and have not requested safe syncing,
  // then sync all chunks with MS_ASYNC.
  if (!(flags & (sync_safe | sync_sloppy)))
    if (m_manager->safe_sync() || m_slotFreeDiskspace() <= m_manager->safe_free_diskspace())
      flags |= sync_safe;
    else
      flags |= sync_force;

  // TODO: This won't trigger for default sync_force.
  if ((flags & sync_use_timeout) && !(flags & sync_force))
    split = partition_optimize(split, m_queue.end(), 50, 5, false);

  uint32_t failed = 0;

  for (Queue::iterator itr = split, last = m_queue.end(); itr != last; ++itr) {
    
    // We can easily skip pieces by swap_iter, so there should be no
    // problem being selective about the ranges we sync.

    // Use a function for checking the next few chunks and see how far
    // we want to sync. When we want to sync everything use end. Call
    // before the loop, or add a check.

    // if we don't want to sync, swap and break.

    std::pair<int,bool> options = sync_options(*itr, flags);

    if (!sync_chunk(*itr, options)) {
      std::iter_swap(itr, split++);
      
      failed++;
      continue;
    }

    (*itr)->set_sync_triggered(true);

    if (!options.second)
      std::iter_swap(itr, split++);
  }

  m_queue.erase(split, m_queue.end());

  // The caller must either make sure that it is safe to close the
  // download or set the sync_ignore_error flag.
  if (failed && !(flags & sync_ignore_error))
    m_slotStorageError("Could not sync chunk: " + std::string(rak::error_number::current().c_str()));

  return failed;
}

std::pair<int, bool>
ChunkList::sync_options(ChunkListNode* node, int flags) {
  // Using if statements since some linkers have problem with static
  // const int members inside the ?: operators. The compiler should
  // be optimizing this anyway.

  if (flags & sync_force) {

    if (flags & sync_safe)
      return std::make_pair(MemoryChunk::sync_sync, true);
    else
      return std::make_pair(MemoryChunk::sync_async, true);

  } else if (flags & sync_safe) {
      
    if (node->sync_triggered())
      return std::make_pair(MemoryChunk::sync_sync, true);
    else
      return std::make_pair(MemoryChunk::sync_async, false);

  } else {
    return std::make_pair(MemoryChunk::sync_async, true);
  }
}

// Using a rather simple algorithm for now. This should really be more
// robust against holes withing otherwise compact ranges and take into
// consideration chunk size.
inline ChunkList::Queue::iterator
ChunkList::seek_range(Queue::iterator first, Queue::iterator last) {
  uint32_t prevIndex = (*first)->index();

  while (++first != last) {
    if ((*first)->index() - prevIndex > 5)
      break;

    prevIndex = (*first)->index();
  }

  return first;
}

inline bool
ChunkList::check_node(ChunkListNode* node) {
  return
    node->time_modified() != rak::timer() &&
    node->time_modified() + rak::timer::from_seconds(m_manager->timeout_sync()) < cachedTime;
}

// Optimize the selection of chunks to sync. Continuous regions are
// preferred, while if too fragmented or if too few chunks are
// available it skips syncing of all chunks.

ChunkList::Queue::iterator
ChunkList::partition_optimize(Queue::iterator first, Queue::iterator last, int weight, int maxDistance, bool dontSkip) {
  for (Queue::iterator itr = first; itr != last;) {
    Queue::iterator range = seek_range(itr, last);

    bool required = std::find_if(itr, range, std::bind1st(std::mem_fun(&ChunkList::check_node), this)) != range;
    dontSkip = dontSkip || required;

    if (!required && std::distance(itr, range) < maxDistance) {
      // Don't sync this range.
      unsigned int l = std::min(range - itr, itr - first);
      std::swap_ranges(first, first + l, range - l);

      first += l;

    } else {
      // This probably increases too fast.
      weight -= std::distance(itr, range) * std::distance(itr, range);
    }

    itr = range;
  }

  // These values are all arbritrary...
  if (!dontSkip && weight > 0)
    return last;

  return first;
}

}
