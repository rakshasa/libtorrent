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

#ifndef LIBTORRENT_ENTRY_LIST_H
#define LIBTORRENT_ENTRY_LIST_H

#include <vector>
#include <rak/functional.h>

#include "torrent/path.h"

namespace torrent {

class Chunk;
class File;
class FileManager;
class MemoryChunk;

class EntryList : private std::vector<File*> {
public:
  typedef std::vector<File*>                          base_type;
  typedef std::vector<std::string>                    path_list;
  typedef std::pair<uint32_t, uint32_t>               range_type;
  typedef rak::mem_fun1<FileManager, void, FileMeta*> slot_meta_type;

  using base_type::value_type;

  using base_type::iterator;
  using base_type::reverse_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::back;
  using base_type::empty;
  using base_type::reserve;

  EntryList() : m_bytesSize(0), m_isOpen(false) {}
  ~EntryList() { clear(); }

  bool                is_open() const                            { return m_isOpen; }

  // We take over ownership of 'file'.
  void                push_back(const Path& path, const range_type& range, off_t size);

  void                clear();

  // Only closes the files.
  void                open();
  void                close();

  // You must call set_root_dir after all nodes have been added.
  const std::string&  root_dir() const                           { return m_rootDir; }
  void                set_root_dir(const std::string& path);

  bool                resize_all();

  size_t              files_size() const                         { return base_type::size(); }
  off_t               bytes_size() const                         { return m_bytesSize; }

  // If the files span multiple disks, the one with the least amount
  // of free diskspace will be returned.
  uint64_t            free_diskspace() const;

  File*               get_node(uint32_t idx)                     { return base_type::operator[](idx); }

  Chunk*              create_chunk(off_t offset, uint32_t length, int prot);

  iterator            at_position(iterator itr, off_t offset);

  path_list*          indirect_links()                           { return &m_indirectLinks; }

  void                slot_insert_filemeta(slot_meta_type s)     { m_slotInsertFileMeta = s; }
  void                slot_erase_filemeta(slot_meta_type s)      { m_slotEraseFileMeta = s; }

private:
  bool                open_file(File* node, const Path& lastPath);

  inline void         make_directory(Path::const_iterator pathBegin, Path::const_iterator pathEnd, Path::const_iterator startItr);

  inline MemoryChunk  create_chunk_part(iterator itr, off_t offset, uint32_t length, int prot);

  off_t               m_bytesSize;
  std::string         m_rootDir;

  bool                m_isOpen;

  path_list           m_indirectLinks;

  slot_meta_type      m_slotInsertFileMeta;
  slot_meta_type      m_slotEraseFileMeta;
};

}

#endif
