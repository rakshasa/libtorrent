#include "config.h"

#include "torrent/exceptions.h"
#include "hash_queue.h"
#include "hash_chunk.h"
#include "storage_chunk.h"
#include "settings.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
void
HashQueue::add(Chunk c, SlotDone d, const std::string& id) {
  if (!c.is_valid() || !c->is_valid())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  HashChunk* hc = new HashChunk(c);

  if (m_chunks.empty()) {
    if (in_service(0))
      throw internal_error("Empty HashQueue is still in service");

    m_tries = 0;
    hc->willneed(hc->remaining());
    insert_service(Timer::current(), 0);
  }

  m_chunks.push_back(Node(hc, id, d));
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
HashQueue::service(int type) {
  while (!m_chunks.empty()) {
    if (!check(++m_tries >= 3))
      return insert_service(Timer::current() + Settings::hashForcedWait, 0);
    
    m_tries = 0;
  }
}

bool
HashQueue::check(bool force) {
  HashChunk* chunk = m_chunks.front().m_chunk;
  
  // TODO: Don't go so far when forcing.
  chunk->perform(chunk->remaining(), force);

  if (chunk->remaining())
    return false;

  m_chunks.front().m_done(chunk->get_chunk(), chunk->get_hash());

  delete chunk;
  m_chunks.pop_front();

  // This should be a few chunks ahead.
  if (!m_chunks.empty())
    m_chunks.front().m_chunk->willneed(m_chunks.front().m_chunk->remaining());

  return true;
}

uint32_t
HashQueue::Node::get_index() {
  return m_chunk->get_chunk()->get_index();
}

}
