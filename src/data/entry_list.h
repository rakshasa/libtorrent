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

#ifndef LIBTORRENT_ENTRY_LIST_H
#define LIBTORRENT_ENTRY_LIST_H

#include <vector>
#include <sigc++/slot.h>

#include "entry_list_node.h"

namespace torrent {

class Chunk;
class MemoryChunk;

class EntryList : private std::vector<EntryListNode> {
public:
  typedef std::vector<EntryListNode>                 Base;
  typedef sigc::slot1<FileMeta*, const std::string&> SlotFileMetaString;
  typedef sigc::slot1<void, FileMeta*>               SlotFileMeta;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::back;
  using Base::empty;
  using Base::reserve;

  EntryList() : m_bytesSize(0) {}
  ~EntryList() { clear(); }

  // We take over ownership of 'file'.
  void                push_back(const Path& path, const EntryListNode::Range& range, off_t size);

  void                clear();

  // Only closes the files.
  void                open(const std::string& root);
  void                close();

  bool                resize_all();

  size_t              get_files_size() const                     { return Base::size(); }
  off_t               get_bytes_size() const                     { return m_bytesSize; }

  EntryListNode*      get_node(uint32_t idx)                     { return &Base::front() + idx; }

  Chunk*              create_chunk(off_t offset, uint32_t length, int prot);

  iterator            at_position(iterator itr, off_t offset);

  void                slot_insert_filemeta(SlotFileMetaString s) { m_slotInsertFileMeta = s; }
  void                slot_erase_filemeta(SlotFileMeta s)        { m_slotEraseFileMeta = s; }

private:
  static bool         open_file(const std::string& root, FileMeta* f, const Path& p, const Path& lastPath);

  inline void         create_chunk_part(MemoryChunk& chunk, iterator itr, off_t offset, uint32_t length, int prot);

  off_t               m_bytesSize;

  SlotFileMetaString  m_slotInsertFileMeta;
  SlotFileMeta        m_slotEraseFileMeta;
};

}

#endif

