#include "config.h"

#include "exceptions.h"
#include "hash_torrent.h"

#include <algo/algo.h>

namespace torrent {

SignalDone& HashTorrent::add(const std::string& id, Storage* storage, HashQueue::SlotDone& slotDone) {
  if (storage == NULL)
    throw internal_error("HashTorrent received Storage NULL pointer");

  List::iterator itr = m_list.insert(m_list.end(), Node(id, storage, slotDone));

  if (itr == m_list.begin()) {
    m_position = 0;
    m_outstanding = 0;

    queue(50);
  }
  
  return itr->torrentDone;
}
  
void HashQueue::clear() {
  std::for_each(m_list.begin(), m_list.end(),
		call_member(ref(*m_queue),
			    &HashQueue::remove,
			    
			    member(&Node::id)));

  m_list.clear();
}

void HashQueue::remove(const std::string& id) {
  List::iterator itr = std::find_if(m_list.begin(), m_list.end(),
				    eq(ref(id), member(&Node::id)));

  if (itr == m_list.end())
    return;

  m_queue->remove(itr->id);

  m_list.erase(itr);
}

void HashQueue::receive_chunkdone(std::string id, unsigned int index, std::string hash) {
}

void HashQueue::queue(unsigned int s) {
  
