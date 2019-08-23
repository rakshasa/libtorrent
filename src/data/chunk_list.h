// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include <functional>
#include <string>
#include <vector>

#include "chunk.h"
#include "chunk_handle.h"
#include "chunk_list_node.h"

namespace torrent {

class ChunkManager;
class Content;
class download_data;
class DownloadWrapper;
class FileList;

class ChunkList : private std::vector<ChunkListNode> {
public:
  typedef uint32_t                            size_type;
  typedef std::vector<ChunkListNode>          base_type;
  typedef std::vector<ChunkListNode*>         Queue;

  typedef std::function<Chunk* (uint32_t, int)>    slot_chunk_index;
  typedef std::function<uint64_t ()>               slot_value;
  typedef std::function<void (const std::string&)> slot_string;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::size;
  using base_type::empty;
  using base_type::operator[];

  static const int sync_all          = (1 << 0);
  static const int sync_force        = (1 << 1);
  static const int sync_safe         = (1 << 2);
  static const int sync_sloppy       = (1 << 3);
  static const int sync_use_timeout  = (1 << 4);
  static const int sync_ignore_error = (1 << 5);

  static const int get_writable      = (1 << 0);
  static const int get_blocking      = (1 << 1);
  static const int get_dont_log      = (1 << 2);
  static const int get_nonblock      = (1 << 3);

  static const int flag_active       = (1 << 0);

  ChunkList() : m_data(NULL), m_manager(NULL), m_flags(0), m_chunk_size(0) {}
  ~ChunkList() { clear(); }

  int                 flags() const                       { return m_flags; }

  void                set_flags(int flags)                { m_flags |= flags; }
  void                unset_flags(int flags)              { m_flags &= ~flags; }
  void                change_flags(int flags, bool state) { if (state) set_flags(flags); else unset_flags(flags); }

  uint32_t            chunk_size() const                  { return m_chunk_size; }
  size_type           queue_size() const                  { return m_queue.size(); }

  download_data*      data()                              { return m_data; }

  void                set_data(download_data* data)       { m_data = data; }
  void                set_manager(ChunkManager* manager)  { m_manager = manager; }
  void                set_chunk_size(uint32_t cs)         { m_chunk_size = cs; }

  bool                has_chunk(size_type index, int prot) const;

  void                resize(size_type to_size);
  void                clear();

  ChunkHandle         get(size_type index, int flags = 0);
  void                release(ChunkHandle* handle, int flags = 0);

  // Replace use_timeout with something like performance related
  // keyword. Then use that flag to decide if we should skip
  // non-continious regions.

  // Returns the number of failed syncs.
  uint32_t            sync_chunks(int flags);

  slot_string&        slot_storage_error()  { return m_slot_storage_error; }
  slot_chunk_index&   slot_create_chunk()   { return m_slot_create_chunk; }
  slot_value&         slot_free_diskspace() { return m_slot_free_diskspace; }

  typedef std::pair<iterator, Chunk::iterator> chunk_address_result;

  chunk_address_result find_address(void* ptr);

private:
  inline bool         is_queued(ChunkListNode* node);

  inline void         clear_chunk(ChunkListNode* node, int flags = 0);
  inline bool         sync_chunk(ChunkListNode* node, std::pair<int,bool> options);

  Queue::iterator     partition_optimize(Queue::iterator first, Queue::iterator last, int weight, int maxDistance, bool dontSkip);

  inline Queue::iterator seek_range(Queue::iterator first, Queue::iterator last);
  inline bool            check_node(ChunkListNode* node);

  std::pair<int,bool> sync_options(ChunkListNode* node, int flags);

  download_data*      m_data;
  ChunkManager*       m_manager;
  Queue               m_queue;

  int                 m_flags;
  uint32_t            m_chunk_size;

  slot_string         m_slot_storage_error;
  slot_chunk_index    m_slot_create_chunk;
  slot_value          m_slot_free_diskspace;
};

}

#endif
