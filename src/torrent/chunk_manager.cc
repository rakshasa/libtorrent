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

#include "data/chunk_list.h"

#include "exceptions.h"
#include "chunk_manager.h"

namespace torrent {

ChunkManager::ChunkManager() :
  m_autoMemory(true),
  m_memoryUsage(0),
  m_lastFreed(0) {

  m_maxMemoryUsage = estimate_max_memory_usage();
}

ChunkManager::~ChunkManager() {
  if (m_memoryUsage != 0)
    throw internal_error("ChunkManager::~ChunkManager() m_memoryUsage != 0.");
}

uint64_t
ChunkManager::estimate_max_memory_usage() {
  // Check ulimit and word size.

  return (uint64_t)1 << 30;
}

uint64_t
ChunkManager::safe_free_diskspace() const {
  // Add some magic here, check how much has been write allocated.

  return (uint64_t)1 << 30;
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
  if (m_memoryUsage + size > m_maxMemoryUsage)
    try_free_memory(size, false);

  if (m_memoryUsage + size > m_maxMemoryUsage)
    try_free_memory(size, true);

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
ChunkManager::try_free_memory(uint64_t size, bool aggressive) {
  uint64_t target = size <= m_memoryUsage ? (m_memoryUsage - size) : 0;

  int flags;

  if (aggressive)
    flags = ChunkList::sync_all | ChunkList::sync_force;
  else
    flags = 0;

  m_lastFreed = std::min<size_type>(m_lastFreed + 1, base_type::size());

  for (iterator itr = begin() + m_lastFreed, last = base_type::end(); itr != last; ++itr) {
    (*itr)->sync_chunks(flags);

    if (m_memoryUsage <= target) {
      m_lastFreed = itr - begin();
      return;
    }
  }

  for (iterator itr = begin(), last = base_type::begin() + m_lastFreed; itr != last; ++itr) {
    (*itr)->sync_chunks(flags);

    if (m_memoryUsage <= target) {
      m_lastFreed = itr - begin();
      return;
    }
  }
}

}
