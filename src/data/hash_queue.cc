#include "config.h"

#include "data/hash_queue.h"

#include <functional>
#include <utility>

#include "data/hash_chunk.h"
#include "data/thread_disk.h"
#include "torrent/data/download_data.h"
#include "torrent/exceptions.h"
#include "torrent/utils/log.h"

#define LT_LOG_DATA(data, log_level, log_fmt, ...)                       \
  lt_log_print_data(LOG_STORAGE_##log_level, data, "hash_queue", log_fmt, __VA_ARGS__);

namespace torrent {

struct HashQueueEqual {
  HashQueueEqual(HashQueueNode::id_type id, uint32_t index) : m_id(id), m_index(index) {}

  bool operator () (const HashQueueNode& q) const { return m_id == q.id() && m_index == q.get_index(); }

  HashQueueNode::id_type m_id;
  uint32_t              m_index;
};

struct HashQueueWillneed {
  HashQueueWillneed(int bytes) : m_bytes(bytes) {}

  bool operator () (HashQueueNode& q) { return (m_bytes -= q.call_willneed()) <= 0; }

  int m_bytes;
};

// If madvise is not available it will always mark the pages as being
// in memory, thus we don't need to modify m_maxTries to have full
// disk usage. But this may cause too much blocking as it will think
// everything is in memory, thus we need to throttle.

// If we're done immediately, move the chunk to the front of the list so
// the next work cycle gets stuff done.
void
HashQueue::push_back(ChunkHandle handle, HashQueueNode::id_type id, slot_done_type d) {
  LT_LOG_DATA(id, DEBUG, "Adding index:%" PRIu32 " to queue.", handle.index());

  if (!handle.is_loaded())
    throw internal_error("HashQueue::add(...) received an invalid chunk");

  auto hash_chunk = new HashChunk(handle);

  base_type::push_back(HashQueueNode(id, hash_chunk, std::move(d)));

  thread_disk()->hash_check_queue()->push_back(hash_chunk);
  thread_disk()->interrupt();
}

bool
HashQueue::has(HashQueueNode::id_type id) {
  return std::any_of(begin(), end(), [id](const auto& n) { return id == n.id(); });
}

bool
HashQueue::has(HashQueueNode::id_type id, uint32_t index) {
  return std::any_of(begin(), end(), HashQueueEqual(id, index));
}

void
HashQueue::remove(HashQueueNode::id_type id) {
  base_type::erase(std::remove_if(begin(), end(), [id, this](auto& itr) {
    if (itr.id() != id)
      return false;

    HashChunk *hash_chunk = itr.get_chunk();

    LT_LOG_DATA(id, DEBUG, "Removing index:%" PRIu32 " from queue.", hash_chunk->handle().index());

    bool result = thread_disk()->hash_check_queue()->remove(hash_chunk);

    // The hash chunk was not found, so we need to wait until the hash
    // check finishes.
    if (!result) {
      auto lock = std::unique_lock(m_done_chunks_lock);

      m_cv.wait(lock, [this, hash_chunk] { return m_done_chunks.find(hash_chunk) != m_done_chunks.end(); });
      m_done_chunks.erase(hash_chunk);
    }

    itr.slot_done()(*hash_chunk->chunk(), NULL);
    itr.clear();

    return true;
  }), end());

}

void
HashQueue::clear() {
  if (!empty())
    throw internal_error("HashQueue::clear() called but valid nodes were found.");

  // Replace with a dtor check to ensure it is empty?
  //   std::for_each(begin(), end(), std::mem_fn(&HashQueueNode::clear));
  //   base_type::clear();
}

void
HashQueue::work() {
  auto lock = std::scoped_lock(m_done_chunks_lock);

  while (!m_done_chunks.empty()) {
    HashChunk* hash_chunk = m_done_chunks.begin()->first;
    HashString hash_value = m_done_chunks.begin()->second;
    m_done_chunks.erase(m_done_chunks.begin());

    // TODO: This is not optimal as we jump around... Check for front
    // of HashQueue in done_chunks instead.

    auto itr = std::find_if(begin(), end(), [hash_chunk](auto& node) { return node.get_chunk() == hash_chunk; });

    // TODO: Fix this...
    if (itr == end())
      throw internal_error("Could not find done chunk's node.");

    LT_LOG_DATA(itr->id(), DEBUG, "Passing index:%" PRIu32 " to owner: %s.",
                hash_chunk->handle().index(),
                hash_string_to_hex_str(hash_value).c_str());

    HashQueueNode::slot_done_type slotDone = itr->slot_done();
    base_type::erase(itr);

    slotDone(hash_chunk->handle(), hash_value.c_str());
    delete hash_chunk;
  }
}

void
HashQueue::chunk_done(HashChunk* hash_chunk, const HashString& hash_value) {
  auto lock = std::scoped_lock(m_done_chunks_lock);

  m_done_chunks[hash_chunk] = hash_value;
  m_slot_has_work(m_done_chunks.empty());
  m_cv.notify_all();
}

} // namespace torrent
