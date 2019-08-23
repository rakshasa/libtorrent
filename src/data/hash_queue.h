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

#ifndef LIBTORRENT_DATA_HASH_QUEUE_H
#define LIBTORRENT_DATA_HASH_QUEUE_H

#include <deque>
#include <functional>
#include <map>
#include <pthread.h>

#include "torrent/hash_string.h"
#include "hash_queue_node.h"
#include "chunk_handle.h"

namespace torrent {

class HashChunk;
class thread_disk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class lt_cacheline_aligned HashQueue : private std::deque<HashQueueNode> {
public:
  typedef std::deque<HashQueueNode>                 base_type;
  typedef std::map<HashChunk*, torrent::HashString> done_chunks_type;

  typedef HashQueueNode::slot_done_type   slot_done_type;
  typedef std::function<void (bool)> slot_bool;

  using base_type::iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::front;
  using base_type::back;

  HashQueue(thread_disk* thread);
  ~HashQueue() { clear(); pthread_mutex_destroy(&m_done_chunks_lock); }

  void                push_back(ChunkHandle handle, HashQueueNode::id_type id, slot_done_type d);

  bool                has(HashQueueNode::id_type id);
  bool                has(HashQueueNode::id_type id, uint32_t index);

  void                remove(HashQueueNode::id_type id);
  void                clear();

  void                work();

  slot_bool&          slot_has_work() { return m_slot_has_work; }

private:
  void                chunk_done(HashChunk* hash_chunk, const HashString& hash_value);

  thread_disk*        m_thread_disk;

  done_chunks_type    m_done_chunks;
  slot_bool           m_slot_has_work;

  pthread_mutex_t     m_done_chunks_lock lt_cacheline_aligned;
};

}

#endif
