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
void HashQueue::service(int type) {
  bool forced = false;

 hashqueue_service_loop:

  if (m_chunks.empty())
    return;
  else if (forced)
    return insert_service(Timer::current() + Settings::hashForcedWait, 0);

  HashChunk* chunk = m_chunks.front().m_chunk;

  unsigned int done = chunk->remaining();

  chunk->perform(chunk->remaining(), false);

  done -= chunk->remaining();

#if USE_MADVISE_WILLNEED
  if (chunk->remaining())
    if (m_tries++ < 3) {
      chunk->willneed(chunk->remaining());
      
      return insert_service(Timer::current() + Settings::hashMadviceWait, 0);
    } else {
      chunk->perform(chunk->remaining(), forced = true);
    }
#else
  if (chunk->remaining())
    chunk->perform(1 << 16, forced = true);
#endif

  if (chunk->remaining())
    // Not done yet, reschedule service. Atleast 10ms so the disk head has time to relocate.
    return insert_service(Timer::current() + 10000, 0);

  m_chunks.front().m_done(chunk->get_chunk(), chunk->get_hash());

  delete chunk;
  m_chunks.pop_front();

  m_tries = 0;

  goto hashqueue_service_loop;
}  

uint32_t HashQueue::Node::get_index() {
  return m_chunk->get_chunk()->get_index();
}

}
