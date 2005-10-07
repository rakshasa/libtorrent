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

#include "torrent/exceptions.h"
#include "hash_queue.h"
#include "hash_chunk.h"
#include "chunk.h"
#include "chunk_list_node.h"

namespace torrent {

struct HashQueueEqual {
  HashQueueEqual(const std::string& id, uint32_t index) : m_id(id), m_index(index) {}

  bool operator () (const HashQueueNode& q) const { return m_id == q.get_id() && m_index == q.get_index(); }

  const std::string&  m_id;
  uint32_t            m_index;
};

struct HashQueueWillneed {
  HashQueueWillneed(int bytes) : m_bytes(bytes) {}

  bool operator () (HashQueueNode& q) { return (m_bytes -= q.call_willneed()) <= 0; }

  int m_bytes;
};

HashQueue::HashQueue() :
  m_readAhead(10 << 20),
  m_interval(10000),
  m_maxTries(20) {

  m_taskWork.set_slot(sigc::mem_fun(*this, &HashQueue::work));
  m_taskWork.set_iterator(taskScheduler.end());
}


inline void
HashQueue::willneed(int bytes) {
  std::find_if(begin(), end(), HashQueueWillneed(bytes));
}

// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
void
HashQueue::push_back(ChunkHandle handle, SlotDone d, const std::string& id) {
  if (!handle.is_valid())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  HashChunk* hc = new HashChunk(handle);

  if (empty()) {
    if (taskScheduler.is_scheduled(&m_taskWork))
      throw internal_error("Empty HashQueue is still in task schedule");

    m_tries = 0;
    taskScheduler.insert(&m_taskWork, Timer::cache());
  }

  Base::push_back(HashQueueNode(hc, id, d));
  willneed(m_readAhead);
}

bool
HashQueue::has(const std::string& id, uint32_t index) {
  return std::find_if(begin(), end(), HashQueueEqual(id, index)) != end();
}

void
HashQueue::remove(const std::string& id) {
  iterator itr = begin();
  
  while ((itr = std::find(itr, end(), id)) != end()) {
    itr->slot_done()(itr->get_chunk()->get_chunk(), "");

    itr->clear();
    itr = erase(itr);
  }

  if (empty())
    taskScheduler.erase(&m_taskWork);
}

void
HashQueue::clear() {
  if (!empty())
    throw internal_error("HashQueue::clear() called but valid nodes were found.");

  // Replace with a dtor check to ensure it is empty?
//   std::for_each(begin(), end(), std::mem_fun_ref(&HashQueueNode::clear));
//   Base::clear();
//   taskScheduler.erase(&m_taskWork);
}

void
HashQueue::work() {
  if (empty())
    return;

  if (!check(++m_tries >= m_maxTries))
    return taskScheduler.insert(&m_taskWork, Timer::cache() + m_interval);

  if (!empty() && !taskScheduler.is_scheduled(&m_taskWork))
    taskScheduler.insert(&m_taskWork, Timer::cache());

  m_tries = std::min(0, m_tries - 2);
}

bool
HashQueue::check(bool force) {
  if (!Base::front().perform(force)) {
    willneed(m_readAhead);
    return false;
  }

  HashChunk* chunk                 = Base::front().get_chunk();
  HashQueueNode::SlotDone slotDone = Base::front().slot_done();

  Base::pop_front();

  slotDone(chunk->get_chunk(), chunk->get_hash());
  delete chunk;

  // This should be a few chunks ahead.
  if (!empty())
    willneed(m_readAhead);

  return true;
}

}
