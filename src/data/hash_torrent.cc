#include "config.h"

#include "torrent/exceptions.h"
#include "storage.h"
#include "hash_torrent.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

void
HashTorrent::add(const std::string& id, Storage* storage, SlotDone torrentDone, HashQueue::SlotDone slotDone) {
  if (storage == NULL)
    throw internal_error("HashTorrent received Storage NULL pointer");

  List::iterator itr = m_list.insert(m_list.end(), Node(id, storage, torrentDone, slotDone));

  // TODO: This check should propably be stored, keep a seperate pos for each
  // torrent. Work will be wasted otherwise.

  // Check if the files have not been allocated.
  unsigned int pos = 0;
    
  while (pos != itr->storage->get_chunkcount()) {
    Storage::Chunk c = itr->storage->get_chunk(pos++);

    if (c.is_valid() && c->is_valid())
      break;
  }

  if (pos == itr->storage->get_chunkcount()) {
    itr->torrentDone();

    m_list.erase(itr);

  } else if (itr == m_list.begin()) {
    m_position = 0;
    m_outstanding = 0;

    queue(50);
  }
}
  
void
HashTorrent::clear() {
  std::for_each(m_list.begin(), m_list.end(),
		call_member(ref(*m_queue),
			    &HashQueue::remove,
			    
			    member(&Node::id)));

  m_list.clear();
}

void
HashTorrent::remove(const std::string& id) {
  List::iterator itr = std::find_if(m_list.begin(), m_list.end(),
				    eq(ref(id), member(&Node::id)));

  if (itr == m_list.end())
    return;

  m_queue->remove(itr->id);
  m_list.erase(itr);
}

void
HashTorrent::receive_chunkdone(HashQueue::Chunk c, std::string hash) {
  if (m_list.empty())
    throw internal_error("HashTorrent::receive_chunkdone is in a mangled state. (Bork, Bork, Bork)");

  // Make sure we call chunkdone before torrentDone has a chance to
  // trigger.
  m_list.front().chunkDone(c, hash);

  if (--m_outstanding == 0 && m_position == m_list.front().storage->get_chunkcount()) {
    m_list.front().torrentDone();
    m_list.pop_front();

    m_position = 0;
  }

  if (!m_list.empty())
    queue(50);
}

void
HashTorrent::queue(unsigned int s) {
  if (m_list.empty())
    throw internal_error("HashTorrent::queue called but there are no queued torrents");

  while (m_outstanding < s &&
	 m_position < m_list.front().storage->get_chunkcount()) {
    
    HashQueue::Chunk c = m_list.front().storage->get_chunk(m_position++);

    if (!c.is_valid() || !c->is_valid())
      continue;

    m_queue->add(m_list.front().id, c, sigc::mem_fun(*this, &HashTorrent::receive_chunkdone));

    m_outstanding++;
  }
}

}
