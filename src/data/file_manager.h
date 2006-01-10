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

#ifndef LIBTORRENT_DATA_FILE_MANAGER_H
#define LIBTORRENT_DATA_FILE_MANAGER_H

#include <rak/unordered_vector.h>

namespace torrent {

class FileMeta;

// Use unordered_vector instead.

class FileManager : private rak::unordered_vector<FileMeta*> {
public:
  typedef rak::unordered_vector<FileMeta*> Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;
  using Base::empty;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  FileManager() : m_openSize(0), m_maxSize(0) {}
  ~FileManager();

  FileMeta*           insert(const std::string& path);
  void                erase(FileMeta* f);

  size_t              open_size() const               { return m_openSize; }

  size_t              max_size() const                { return m_maxSize; }
  void                set_max_size(size_t s);

private:
  FileManager(const FileManager&);
  void operator = (const FileManager&);

  // Bool or throw? iterator or reference/pointer?
  bool                prepare_file(FileMeta* meta, int prot, int flags);
  void                close_file(FileMeta* meta);

  void                close_least_active();

  size_t              m_openSize;
  size_t              m_maxSize;
};

}

#endif
