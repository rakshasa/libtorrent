#include "config.h"

#include "hash_check_queue.h"

#include "data/hash_chunk.h"
#include "torrent/hash_string.h"
#include "utils/instrumentation.h"

namespace torrent {

HashCheckQueue::HashCheckQueue()  = default;
HashCheckQueue::~HashCheckQueue() = default;

// Always poke thread_disk after calling this.
void
HashCheckQueue::push_back(HashChunk* hash_chunk) {
  if (hash_chunk == NULL || !hash_chunk->chunk()->is_loaded() || !hash_chunk->chunk()->is_blocking())
    throw internal_error("Invalid hash chunk passed to HashCheckQueue.");

  auto lock = std::scoped_lock(m_lock);

  // Set blocking...(? this needs to be possible to do after getting
  // the chunk) When doing this make sure we verify that the handle is
  // not previously blocked.

  base_type::push_back(hash_chunk);

  int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
  instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, 1);
  instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, size);
}

// erase...
//
// The erasing function should call slot, perhaps return a bool if we
// deleted, or in the rare case it has already been hashed and will
// arraive in the near future?
//
// We could handle this by polling the done chunks on false return
// values.

// Lock the chunk list and increment blocking only when starting the
// checking.

// No removal of entries, only clearing.

bool
HashCheckQueue::remove(HashChunk* hash_chunk) {
  auto lock = std::scoped_lock(m_lock);

  bool result;
  auto itr = std::find(begin(), end(), hash_chunk);

  if (itr != end()) {
    base_type::erase(itr);
    result = true;

    int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, -1);
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, -size);

  } else {
    result = false;
  }

  return result;
}

void
HashCheckQueue::perform() {
  auto lock = std::unique_lock(m_lock);

  while (!empty()) {
    HashChunk* hash_chunk = base_type::front();
    base_type::pop_front();

    if (!hash_chunk->chunk()->is_loaded())
      throw internal_error("HashCheckQueue::perform(): !entry.node->is_loaded().");

    int64_t size = hash_chunk->chunk()->chunk()->chunk_size();
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT, -1);
    instrumentation_update(INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE, -size);

    lock.unlock();

    if (!hash_chunk->perform(~uint32_t(), true))
      throw internal_error("HashCheckQueue::perform(): !hash_chunk->perform(~uint32_t(), true).");

    HashString hash;
    hash_chunk->hash_c(hash.data());

    m_slot_chunk_done(hash_chunk, hash);
    lock.lock();
  }
}

} // namespace torrent
