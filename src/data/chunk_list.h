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

#ifndef LIBTORRENT_DATA_CHUNK_LIST_H
#define LIBTORRENT_DATA_CHUNK_LIST_H

#include <vector>
#include <rak/error_number.h>
#include <rak/functional.h>

#include "chunk_handle.h"
#include "chunk_list_node.h"

namespace torrent {

class ChunkManager;
class Content;
class EntryList;

class ChunkList : private std::vector<ChunkListNode> {
public:
  typedef uint32_t                            size_type;
  typedef std::vector<ChunkListNode>          base_type;
  typedef std::pair<Chunk*,rak::error_number> CreateChunk;
  typedef std::vector<ChunkListNode*>         Queue;

  typedef rak::mem_fun2<Content, CreateChunk, uint32_t, bool> SlotCreateChunk;
  typedef rak::const_mem_fun0<EntryList, uint64_t>            SlotFreeDiskspace;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  static const int sync_all         = (1 << 0);
  static const int sync_force       = (1 << 1);
  static const int sync_safe        = (1 << 2);
  static const int sync_sloppy      = (1 << 3);
  static const int sync_use_timeout = (1 << 4);

  ChunkList() : m_manager(NULL) {}
  ~ChunkList() { clear(); }

  void                set_manager(ChunkManager* manager)      { m_manager = manager; }

  bool                has_chunk(size_type index, int prot) const;

  void                resize(size_type s);
  void                clear();

  ChunkHandle         get(size_type index, bool writable);
  void                release(ChunkHandle* handle);

  size_type           queue_size() const                      { return m_queue.size(); }

  // Replace use_timeout with something like performance related
  // keyword. Then use that flag to decide if we should skip
  // non-continious regions.

  // Returns the number of failed syncs.
  uint32_t            sync_chunks(int flags);

  void                slot_create_chunk(SlotCreateChunk s)     { m_slotCreateChunk = s; }
  void                slot_free_diskspace(SlotFreeDiskspace s) { m_slotFreeDiskspace = s; }

private:
  inline bool         is_queued(ChunkListNode* node);

  inline void         clear_chunk(ChunkListNode* node);
  inline bool         sync_chunk(ChunkListNode* node, int flags, bool cleanup);

  Queue::iterator     partition_optimize(Queue::iterator first, Queue::iterator last, int weight, int maxDistance, bool dontSkip);

  inline Queue::iterator seek_range(Queue::iterator first, Queue::iterator last);
  inline bool            check_node(ChunkListNode* node);

  ChunkManager*       m_manager;
  Queue               m_queue;

  SlotCreateChunk     m_slotCreateChunk;
  SlotFreeDiskspace   m_slotFreeDiskspace;
};

}

#endif
