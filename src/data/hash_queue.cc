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

#include "config.h"

#include "torrent/exceptions.h"
#include "hash_queue.h"
#include "hash_chunk.h"
#include "storage_chunk.h"
#include "settings.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

HashQueue::HashQueue() :
  m_taskWork(sigc::mem_fun(*this, &HashQueue::work)) {
}

// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
void
HashQueue::add(Chunk c, SlotDone d, const std::string& id) {
  if (!c.is_valid() || !c->is_valid())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  HashChunk* hc = new HashChunk(c);

  if (m_chunks.empty()) {
    if (m_taskWork.is_scheduled())
      throw internal_error("Empty HashQueue is still in task schedule");

    m_tries = 0;
    m_taskWork.insert(Timer::current());
  }

  m_chunks.push_back(Node(hc, id, d));
  willneed(Settings::hashWillneed);
}

bool
HashQueue::has(uint32_t index, const std::string& id) {
  return std::find_if(m_chunks.begin(), m_chunks.end(),
		      bool_and(eq(ref(id), member(&HashQueue::Node::m_id)),
			       eq(value(index), call_member(&HashQueue::Node::get_index))))
    != m_chunks.end();
}

void
HashQueue::remove(const std::string& id) {
  ChunkList::iterator itr = m_chunks.begin();
  
  while ((itr = std::find_if(itr, m_chunks.end(), eq(ref(id), member(&Node::m_id))))
	 != m_chunks.end()) {
    delete itr->m_chunk;
    
    itr = m_chunks.erase(itr);
  }
}

void
HashQueue::clear() {
  while (!m_chunks.empty()) {
    delete m_chunks.front().m_chunk;

    m_chunks.pop_front();
  }
}

// TODO: Clean up this code, it's ugly.
void
HashQueue::work() {
  while (!m_chunks.empty()) {
    if (!check(++m_tries >= Settings::hashTries))
      return m_taskWork.insert(Timer::current() + Settings::hashWait);
    
    m_tries = 0;
  }
}

bool
HashQueue::check(bool force) {
  HashChunk* chunk = m_chunks.front().m_chunk;
  
  if (!chunk->perform(chunk->remaining(), force))
    return false;

  m_chunks.front().m_done(chunk->get_chunk(), chunk->get_hash());

  delete chunk;
  m_chunks.pop_front();

  // This should be a few chunks ahead.
  if (!m_chunks.empty())
    willneed(Settings::hashWillneed);

  return true;
}

void
HashQueue::willneed(int count) {
  for (ChunkList::iterator itr = m_chunks.begin(); itr != m_chunks.end() && count--; ++itr)
    if (!itr->m_willneed) {
      itr->m_willneed = true;
      itr->m_chunk->advise_willneed(itr->m_chunk->remaining());
    }
}

uint32_t
HashQueue::Node::get_index() {
  return m_chunk->get_chunk()->get_index();
}

}
