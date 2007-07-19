// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

// Add some helpfull words here.

#ifndef LIBTORRENT_CHUNK_MANAGER_H
#define LIBTORRENT_CHUNK_MANAGER_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT ChunkManager : private std::vector<ChunkList*> {
public:
  typedef std::vector<ChunkList*> base_type;
  typedef uint32_t                size_type;

  typedef base_type::iterator     iterator;

  using base_type::size;
  using base_type::empty;

  ChunkManager();
  ~ChunkManager();
  
  // The ChunkManager will automatically try to adjust max memory
  // usage to a resonable value. This should be based on the arch,
  // ulimit and errors encountered when mmap'ing.
  bool                auto_memory() const                       { return m_autoMemory; }
  void                set_auto_memory(bool state)               { m_autoMemory = state; }

  uint64_t            memory_usage() const                      { return m_memoryUsage; }

  // Should we allow the client to reserve some memory?

  // The client should set this automatically if ulimit is set.
  uint64_t            max_memory_usage() const                  { return m_maxMemoryUsage; }
  void                set_max_memory_usage(uint64_t bytes)      { m_maxMemoryUsage = bytes; }

  // Estimate the max memory usage possible, capped at 1GB.
  static uint64_t     estimate_max_memory_usage();

  uint64_t            safe_free_diskspace() const;

  bool                safe_sync() const                         { return m_safeSync; }
  void                set_safe_sync(uint32_t state)             { m_safeSync = state; }

  // Set the interval to wait after the last write to a chunk before
  // trying to sync it. By not forcing a sync too early it should give
  // the kernel an oppertunity to sync at its convenience.
  uint32_t            timeout_sync() const                      { return m_timeoutSync; }
  void                set_timeout_sync(uint32_t seconds)        { m_timeoutSync = seconds; }

  uint32_t            timeout_safe_sync() const                 { return m_timeoutSafeSync; }
  void                set_timeout_safe_sync(uint32_t seconds)   { m_timeoutSafeSync = seconds; }

  // Set to 0 to disable preloading.
  //
  // How the value is used is yet to be determined, but it won't be
  // able to use actual requests in the request queue as we can easily
  // stay ahead of it causing preloading to fail.
  uint32_t            preload_type() const                      { return m_preloadType; }
  void                set_preload_type(uint32_t t)              { m_preloadType = t; }

  uint32_t            preload_min_size() const                  { return m_preloadMinSize; }
  void                set_preload_min_size(uint32_t bytes)      { m_preloadMinSize = bytes; }

  // Required rate before attempting to preload chunk, per whole
  // megabyte of chunk size.
  uint32_t            preload_required_rate() const             { return m_preloadRequiredRate; }
  void                set_preload_required_rate(uint32_t bytes) { m_preloadRequiredRate = bytes; }


  void                insert(ChunkList* chunkList);
  void                erase(ChunkList* chunkList);

  // The client may use these functions to affect the library's memory
  // usage by indicating how much it uses. This shouldn't really be
  // nessesary unless the client maps large amounts of memory.
  //
  // The primary user of these functions is ChunkList.
  bool                allocate(uint32_t size);
  void                deallocate(uint32_t size);

  void                try_free_memory(uint64_t size);
  
  void                periodic_sync();

  // Not sure if I wnt these here. Consider implementing a generic
  // statistics API.
  uint32_t            stats_preloaded() const                   { return m_statsPreloaded; }
  void                inc_stats_preloaded()                     { m_statsPreloaded++; }

  uint32_t            stats_not_preloaded() const               { return m_statsNotPreloaded; }
  void                inc_stats_not_preloaded()                 { m_statsNotPreloaded++; }

private:
  ChunkManager(const ChunkManager&);
  void operator = (const ChunkManager&);

  void                sync_all(int flags, uint64_t target) LIBTORRENT_NO_EXPORT;

  bool                m_autoMemory;

  uint64_t            m_memoryUsage;
  uint64_t            m_maxMemoryUsage;

  bool                m_safeSync;
  uint32_t            m_timeoutSync;
  uint32_t            m_timeoutSafeSync;

  uint32_t            m_preloadType;
  uint32_t            m_preloadMinSize;
  uint32_t            m_preloadRequiredRate;

  uint32_t            m_statsPreloaded;
  uint32_t            m_statsNotPreloaded;

  int32_t             m_timerStarved;
  size_type           m_lastFreed;
};

}

#endif
