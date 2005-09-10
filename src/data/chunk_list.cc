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

#include "config.h"

#include <rak/functional.h>

#include "torrent/exceptions.h"

#include "chunk_list.h"
#include "chunk.h"

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

  int index = 0;

  for (iterator itr = begin(), last = end(); itr != last; ++itr, ++index)
    itr->set_index(index);
}

void
ChunkList::clear() {
  if (!m_queue.empty())
    throw internal_error("ChunkList::clear() m_queue could not be clear.");

  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::references)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  Base::clear();
}

ChunkHandle
ChunkList::get(size_type index, bool writable) {
  ChunkListNode* node = &Base::at(index);

  if (!node->is_valid() || (writable && !node->chunk()->is_writable())) {
    Chunk* chunk = m_slotCreateChunk(index, writable);

    if (chunk == NULL)
      return ChunkHandle();

    if (node->is_valid())
      delete node->chunk();

    node->set_chunk(chunk);
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

inline void
ChunkList::sync_chunk(ChunkListNode* node) {
  // Check return value?
  node->chunk()->sync(MemoryChunk::sync_async);

  node->dec_writable();
  node->dec_references();
 
  if (node->references() == 0) {
    delete node->chunk();
    node->set_chunk(NULL);
  }
}

void
ChunkList::sync_all() {
  Queue::iterator split = std::partition(m_queue.begin(), m_queue.end(),
					 rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));

  std::for_each(split, m_queue.end(), std::ptr_fun(&ChunkList::sync_chunk));
  m_queue.erase(split, m_queue.end());
}

void
ChunkList::sync_periodic() {
//   Queue::iterator split = std::partition(m_queue.begin(), m_queue.end(),
// 					 rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));

//   std::for_each(split, m_queue.end(), std::ptr_fun(&ChunkList::sync_chunk));
//   m_queue.erase(split, m_queue.end());
  sync_all();
}

}
