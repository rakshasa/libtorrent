// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <sys/types.h>
#include <sys/resource.h>

#include "data/chunk_list.h"

#include "exceptions.h"
#include "chunk_manager.h"
#include "globals.h"

namespace torrent {

ChunkManager::ChunkManager() :
  m_autoMemory(true),
  m_memoryUsage(0),

  m_safeSync(false),
  m_timeoutSync(600),
  m_timeoutSafeSync(900),

  m_preloadType(0),
  m_preloadMinSize(256 << 10),
  m_preloadRequiredRate(5),

  m_statsPreloaded(0),
  m_statsNotPreloaded(0),

  m_timerStarved(0),
  m_lastFreed(0) {

  // 1/5 of the available memory should be enough for the client. If
  // the client really requires alot more memory it should call this
  // itself.
  m_maxMemoryUsage = (estimate_max_memory_usage() * 4) / 5;
}

ChunkManager::~ChunkManager() {
  if (m_memoryUsage != 0)
    throw internal_error("ChunkManager::~ChunkManager() m_memoryUsage != 0.");
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

  return (uint64_t)DEFAULT_ADDRESS_SPACE_SIZE << 20;
}

uint64_t
ChunkManager::safe_free_diskspace() const {
  return m_memoryUsage + ((uint64_t)512 << 20);
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

  iterator itr = std::find(base_type::begin(), base_type::end(), chunkList);

  if (itr == base_type::end())
    throw internal_error("ChunkManager::erase(...) itr == base_type::end().");

  std::iter_swap(itr, --base_type::end());
  base_type::pop_back();

  chunkList->set_manager(NULL);
}

bool
ChunkManager::allocate(uint32_t size) {
  if (m_memoryUsage + size > (3 * m_maxMemoryUsage) / 4)
    try_free_memory((1 * m_maxMemoryUsage) / 4);

  if (m_memoryUsage + size > m_maxMemoryUsage)
    return false;

  m_memoryUsage += size;

  return true;
}

void
ChunkManager::deallocate(uint32_t size) {
  if (size > m_memoryUsage)
    throw internal_error("ChunkManager::deallocate(...) size > m_memoryUsage.");

  m_memoryUsage -= size;
}

void
ChunkManager::try_free_memory(uint64_t size) {
  // Ensure that we don't call this function too often when futile as
  // it might be somewhat expensive.
  //
  // Note that it won't be able to free chunks that are scheduled for
  // hash checking, so a too low max memory setting will give problem
  // at high transfer speed.
  if (m_timerStarved + 10 >= cachedTime.seconds())
    return;

  sync_all(0, size <= m_memoryUsage ? (m_memoryUsage - size) : 0);

  // The caller must ensure he tries to free a sufficiently large
  // amount of memory to ensure it, and other users, has enough memory
  // space for at least 10 seconds.
  m_timerStarved = cachedTime.seconds();
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

  iterator itr = base_type::begin() + m_lastFreed;

  do {
    if (itr == base_type::end())
      itr = base_type::begin();
    
    (*itr)->sync_chunks(flags);

  } while (++itr != base_type::begin() + m_lastFreed && m_memoryUsage >= target);

  m_lastFreed = itr - begin();
}

}
