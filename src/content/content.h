// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <inttypes.h>
#include <string>
#include <sigc++/signal.h>

#include "utils/bitfield.h"
#include "utils/task.h"

#include "data/chunk_list.h"
#include "data/entry_list.h"
#include "data/memory_chunk.h"
#include "data/piece.h"

namespace torrent {

// Since g++ uses reference counted strings, it's cheaper to just hand
// over bencode's string.

// The ranges in the ContentFile elements spans from the first chunk
// they have data on, to the last plus one. This means the range end
// minus one of one file can be the start of one or more other file
// ranges.

class Content {
public:
  typedef sigc::signal0<void>                        Signal;

  // Hash done signal, hash failed signal++
  
  Content();

  // Do not modify chunk size after files have been added.
  void                   add_file(const Path& path, uint64_t size);

  void                   set_complete_hash(const std::string& hash);
  void                   set_root_dir(const std::string& path);

  std::string            get_hash(unsigned int index);
  const char*            get_hash_c(unsigned int index)       { return m_hash.c_str() + 20 * index; }
  const std::string&     get_complete_hash()                  { return m_hash; }
  const std::string&     get_root_dir()                       { return m_rootDir; }

  uint32_t               get_chunks_completed()               { return m_completed; }

  off_t                  get_bytes_size() const               { return m_entryList.get_bytes_size(); }
  uint64_t               get_bytes_completed();
  
  uint32_t               get_chunk_total() const              { return (get_bytes_size() + m_chunkSize - 1) / m_chunkSize; }

  uint32_t               get_chunk_size() const               { return m_chunkSize; }
  void                   set_chunk_size(uint32_t s)           { m_chunkSize = s; }

  uint32_t               get_chunk_index_size(uint32_t index) const;

  off_t                  get_chunk_position(uint32_t c) const { return c * (off_t)m_chunkSize; }

  BitField&              get_bitfield()                       { return m_bitfield; }

  ChunkList*             chunk_list()                         { return &m_chunkList; }
  const ChunkList*       chunk_list() const                   { return &m_chunkList; }

  EntryList*             entry_list()                         { return &m_entryList; }
  const EntryList*       entry_list() const                   { return &m_entryList; }

  bool                   is_open() const                      { return m_isOpen; }
  bool                   is_correct_size();
  bool                   is_done() const                      { return m_completed == get_chunk_total(); }

  bool                   is_valid_piece(const Piece& p) const;

  bool                   has_chunk(uint32_t index) const      { return m_bitfield[index]; }

  ChunkListNode*         get_chunk(uint32_t index, int prot);
  void                   release_chunk(ChunkListNode* node)   { m_chunkList.release(node); }

  void                   open();
  void                   close();

  void                   resize();

  void                   mark_done(uint32_t index);
  void                   update_done();

  Signal&                signal_download_done()               { return m_signalDownloadDone; }
  void                   block_download_done(bool v)          { m_delayDownloadDone.get_slot().block(v); }

private:
  StorageChunk*       get_storage_chunk(uint32_t b, int prot);
  MemoryChunk         get_storage_chunk_part(EntryList::iterator itr, off_t offset, uint32_t length, int prot);
  
  EntryList::iterator    mark_done_file(EntryList::iterator itr, uint32_t index);
  EntryListNode::Range     make_index_range(uint64_t pos, uint64_t size) const;

  bool                   m_isOpen;
  uint32_t               m_completed;
  uint32_t               m_chunkSize;

  ChunkList              m_chunkList;
  EntryList              m_entryList;

  BitField               m_bitfield;

  std::string            m_rootDir;
  std::string            m_hash;

  Signal                 m_signalDownloadDone;
  TaskItem               m_delayDownloadDone;
};

inline EntryList::iterator
Content::mark_done_file(EntryList::iterator itr, uint32_t index) {
  while (index >= itr->get_range().second) ++itr;
  
  do {
    itr->set_completed(itr->get_completed() + 1);
  } while (index + 1 == itr->get_range().second && ++itr != m_entryList.end());

  return itr;
}

inline EntryListNode::Range
Content::make_index_range(uint64_t pos, uint64_t size) const {
  return EntryListNode::Range(pos / m_chunkSize, (pos + size + m_chunkSize - 1) / m_chunkSize);
}

}

#endif
