#include "config.h"

#include "torrent/exceptions.h"
#include "hash_queue.h"
#include "hash_chunk.h"
#include "storage_chunk.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
HashQueue::SignalDone& HashQueue::add(const std::string& id, Chunk c, bool try_immediately) {
  if (!c.is_valid() ||
      !c->is_valid())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  if (m_chunks.empty()) {
    if (in_service(0))
      throw internal_error("Empty HashQueue is still in service");

    m_tries = 0;
    insert_service(Timer::current(), 0);
  }

  ChunkList::iterator itr = m_chunks.insert(m_chunks.end(), Node(new HashChunk(c), id, c->get_index()));

  if (try_immediately) {
    
    while (itr->chunk->remaining() &&
	   itr->chunk->perform(itr->chunk->remaining()));

    if (itr->chunk->remaining() == 0)
      m_chunks.splice(m_chunks.begin(), m_chunks, itr);
  }

  return itr->signal;
}

void HashQueue::remove(const std::string& id) {
  ChunkList::iterator itr = m_chunks.begin();
  
  while ((itr = std::find_if(itr, m_chunks.end(), eq(ref(id), member(&Node::id))))
	 != m_chunks.end()) {
    delete itr->chunk;
    
    itr = m_chunks.erase(itr);
  }
}

void HashQueue::clear() {
  while (!m_chunks.empty()) {
    delete m_chunks.front().chunk;

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
    return insert_service(Timer::current() + 100000, 0);

  HashChunk* chunk = m_chunks.front().chunk;

  unsigned int done = chunk->remaining();

  chunk->perform(chunk->remaining(), false);

  done -= chunk->remaining();

#if USE_MADVISE_WILLNEED
  if (chunk->remaining())
    if (m_tries++ < 3) {
      chunk->willneed(chunk->remaining());
      
      return insert_service(Timer::current() + 50000, 0);
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

  m_chunks.front().signal.emit(m_chunks.front().id,
			       chunk->get_chunk(),
			       chunk->get_hash());

  delete chunk;
  m_chunks.pop_front();

  m_tries = 0;

  goto hashqueue_service_loop;
}  

}
