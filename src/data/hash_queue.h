#ifndef LIBTORRENT_DATA_HASH_QUEUE_H
#define LIBTORRENT_DATA_HASH_QUEUE_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>

#include "torrent/hash_string.h"
#include "hash_queue_node.h"
#include "chunk_handle.h"

namespace torrent {

class HashChunk;
class ThreadDisk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class HashQueue : private std::deque<HashQueueNode> {
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

  HashQueue() = default;
  ~HashQueue() { clear(); }

  void                push_back(ChunkHandle handle, HashQueueNode::id_type id, slot_done_type d);

  bool                has(HashQueueNode::id_type id);
  bool                has(HashQueueNode::id_type id, uint32_t index);

  void                remove(HashQueueNode::id_type id);
  void                clear();

  void                work();

  slot_bool&          slot_has_work() { return m_slot_has_work; }

protected:
  friend class ThreadDisk;

  void                chunk_done(HashChunk* hash_chunk, const HashString& hash_value);

private:
  done_chunks_type    m_done_chunks;
  slot_bool           m_slot_has_work;

  std::mutex              m_done_chunks_lock;
  std::condition_variable m_cv;
};

}

#endif
