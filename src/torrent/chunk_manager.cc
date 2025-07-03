#include "config.h"

#include "torrent/chunk_manager.h"

#include <cassert>
#include <sys/resource.h>

#include "data/chunk_list.h"
#include "torrent/exceptions.h"
#include "utils/instrumentation.h"

namespace torrent {

ChunkManager::ChunkManager() = default;

ChunkManager::~ChunkManager() {
  assert(m_memoryUsage == 0 && "ChunkManager::~ChunkManager() m_memoryUsage != 0.");
  assert(m_memoryBlockCount == 0 && "ChunkManager::~ChunkManager() m_memoryBlockCount != 0.");
}

uint64_t
ChunkManager::sync_queue_memory_usage() const {
  uint64_t size = 0;

  for (auto chunk : *this)
    size += chunk->queue_size() * chunk->chunk_size();

  return size;
}

uint32_t
ChunkManager::sync_queue_size() const {
  uint32_t size = 0;

  for (auto chunk : *this)
    size += chunk->queue_size();

  return size;
}

uint64_t
ChunkManager::estimate_max_memory_usage() {
  rlimit rlp;

#ifdef RLIMIT_AS
  if (getrlimit(RLIMIT_AS, &rlp) == 0 && rlp.rlim_cur != RLIM_INFINITY)
#else
  if (getrlimit(RLIMIT_DATA, &rlp) == 0 && rlp.rlim_cur != RLIM_INFINITY)
#endif
    return rlp.rlim_cur;

  return uint64_t{DEFAULT_ADDRESS_SPACE_SIZE} << 20;
}

uint64_t
ChunkManager::safe_free_diskspace() const {
  return m_memoryUsage + (uint64_t{512} << 20);
}

void
ChunkManager::insert(ChunkList* chunkList) {
  chunkList->set_manager(this);

  base_type::push_back(chunkList);
}

void
ChunkManager::erase(ChunkList* chunkList) {
  if (chunkList->queue_size() != 0)
    throw internal_error("ChunkManager::erase(...) chunkList->queue_size() != 0.");

  auto itr = std::find(base_type::begin(), base_type::end(), chunkList);

  if (itr == base_type::end())
    throw internal_error("ChunkManager::erase(...) itr == base_type::end().");

  std::iter_swap(itr, --base_type::end());
  base_type::pop_back();

  chunkList->set_manager(NULL);
}

bool
ChunkManager::allocate(uint32_t size, int flags) {
  if (m_memoryUsage + size > (3 * m_maxMemoryUsage) / 4)
    try_free_memory((1 * m_maxMemoryUsage) / 4);

  if (m_memoryUsage + size > m_maxMemoryUsage) {
    if (!(flags & allocate_dont_log))
      instrumentation_update(INSTRUMENTATION_MINCORE_ALLOC_FAILED, 1);

    return false;
  }

  if (!(flags & allocate_dont_log))
    instrumentation_update(INSTRUMENTATION_MINCORE_ALLOCATIONS, size);

  m_memoryUsage += size;
  m_memoryBlockCount++;

  instrumentation_update(INSTRUMENTATION_MEMORY_CHUNK_COUNT, 1);
  instrumentation_update(INSTRUMENTATION_MEMORY_CHUNK_USAGE, size);

  return true;
}

void
ChunkManager::deallocate(uint32_t size, int flags) {
  if (size > m_memoryUsage)
    throw internal_error("ChunkManager::deallocate(...) size > m_memoryUsage.");

  if (!(flags & allocate_dont_log)) {
    if (flags & allocate_revert_log)
      instrumentation_update(INSTRUMENTATION_MINCORE_ALLOCATIONS, -size);
    else
      instrumentation_update(INSTRUMENTATION_MINCORE_DEALLOCATIONS, size);
  }

  m_memoryUsage -= size;
  m_memoryBlockCount--;

  instrumentation_update(INSTRUMENTATION_MEMORY_CHUNK_COUNT, -1);
  instrumentation_update(INSTRUMENTATION_MEMORY_CHUNK_USAGE, -static_cast<int64_t>(size));
}

void
ChunkManager::try_free_memory(uint64_t size) {
  // Ensure that we don't call this function too often when futile as
  // it might be somewhat expensive.
  //
  // Note that it won't be able to free chunks that are scheduled for
  // hash checking, so a too low max memory setting will give problem
  // at high transfer speed.
  if (m_timerStarved + 10 >= this_thread::cached_seconds().count())
    return;

  sync_all(0, size <= m_memoryUsage ? (m_memoryUsage - size) : 0);

  // The caller must ensure he tries to free a sufficiently large
  // amount of memory to ensure it, and other users, has enough memory
  // space for at least 10 seconds.
  m_timerStarved = this_thread::cached_seconds().count();
}

void
ChunkManager::periodic_sync() {
  sync_all(ChunkList::sync_use_timeout, 0);
}

void
ChunkManager::sync_all(int flags, uint64_t target) {
  if (empty())
    return;

  // Start from the next entry so that we don't end up repeatedly
  // syncing the same torrent.
  m_lastFreed = m_lastFreed % base_type::size() + 1;

  auto itr = base_type::begin() + m_lastFreed;

  do {
    if (itr == base_type::end())
      itr = base_type::begin();

    (*itr)->sync_chunks(static_cast<ChunkList::sync_flags>(flags));

  } while (++itr != base_type::begin() + m_lastFreed && m_memoryUsage >= target);

  m_lastFreed = itr - begin();
}

} // namespace torrent
