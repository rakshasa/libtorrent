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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DATA_FILE_MANAGER_H
#define LIBTORRENT_DATA_FILE_MANAGER_H

#include <vector>

namespace torrent {

class FileMeta;

// Note: this list may be rearranged during calls to close_files(...),
// so don't assume anything about the order.

class FileManager : private std::vector<FileMeta*> {
public:
  typedef std::vector<FileMeta*> Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  FileManager(size_t max) : m_openSize(0), m_maxSize(max) {}
  ~FileManager() { clear(); }

  void                clear();

  FileMeta*           insert(const std::string& path);

  size_t              open_size() const                  { return m_openSize; }
  size_t              max_size() const                   { return m_maxSize; }

  // Bool or throw? iterator or reference/pointer?
  bool                open_file(FileMeta* meta, int flags);
  void                close_file(FileMeta* meta);

  void                close_files(size_t count);

private:
  FileManager(const FileManager&);
  void operator = (const FileManager&);

  size_t              m_openSize;
  size_t              m_maxSize;
};

}

#endif
