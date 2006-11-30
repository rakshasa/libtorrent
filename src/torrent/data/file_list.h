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

#ifndef LIBTORRENT_FILE_LIST_H
#define LIBTORRENT_FILE_LIST_H

#include <vector>
#include <torrent/common.h>
#include <torrent/path.h>

namespace torrent {

class Content;
class DownloadConstructor;
class DownloadMain;
class DownloadWrapper;

class LIBTORRENT_EXPORT FileList : private std::vector<File*> {
public:
  friend class Content;
  friend class DownloadConstructor;
  friend class DownloadMain;
  friend class DownloadWrapper;

  typedef std::vector<File*>            base_type;
  typedef std::vector<std::string>      path_list;

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

  FileList();

  bool                is_open() const                            { return m_isOpen; }

  // You must call set_root_dir after all nodes have been added.
  const std::string&  root_dir() const                           { return m_rootDir; }
  void                set_root_dir(const std::string& path);

  size_t              size_files() const                         { return base_type::size(); }
  uint64_t            size_bytes() const                         { return m_sizeBytes; }

  uint64_t            max_file_size() const                      { return m_maxFileSize; }
  void                set_max_file_size(uint64_t size);

  uint32_t            chunk_size() const                         { return m_chunkSize; }

  // If the files span multiple disks, the one with the least amount
  // of free diskspace will be returned.
  uint64_t            free_diskspace() const;

  File*               at_index(uint32_t idx)                     { return *(begin() + idx); }
  iterator            at_position(iterator itr, uint64_t offset);

  const path_list*    indirect_links() const                     { return &m_indirectLinks; }

protected:
  void                open() LIBTORRENT_NO_EXPORT;
  void                close() LIBTORRENT_NO_EXPORT;

  void                clear() LIBTORRENT_NO_EXPORT;
  bool                resize_all() LIBTORRENT_NO_EXPORT;

  void                set_chunk_size(uint32_t size)              { m_chunkSize = size; }

  void                push_back(const Path& path, uint64_t fileSize) LIBTORRENT_NO_EXPORT;

  Chunk*              create_chunk(uint64_t offset, uint32_t length, int prot) LIBTORRENT_NO_EXPORT;

  iterator            inc_completed(iterator firstItr, uint64_t firstPos, uint64_t lastPos) LIBTORRENT_NO_EXPORT;

private:
  inline bool         open_file(File* node, const Path& lastPath);
  inline void         make_directory(Path::const_iterator pathBegin, Path::const_iterator pathEnd, Path::const_iterator startItr);
  inline MemoryChunk  create_chunk_part(iterator itr, uint64_t offset, uint32_t length, int prot);

  bool                m_isOpen;

  uint64_t            m_sizeBytes;
  uint32_t            m_chunkSize;
  uint64_t            m_maxFileSize;

  std::string         m_rootDir;

  path_list           m_indirectLinks;
};

}

#endif
