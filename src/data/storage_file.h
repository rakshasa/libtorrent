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

#ifndef LIBTORRENT_STORAGE_FILE_H
#define LIBTORRENT_STORAGE_FILE_H

namespace torrent {

class File;

class StorageFile {
public:
  StorageFile() : m_file(NULL), m_position(0), m_size(0) {}
  StorageFile(File* f, off_t p, off_t s) : m_file(f), m_position(p), m_size(s) {}

  bool                is_valid() const                        { return m_file; }
  inline bool         is_valid_position(off_t p) const;

  void                clear();

  File*               file()                                  { return m_file; }
  const File*         file() const                            { return m_file; }
  off_t               position() const                        { return m_position; }
  off_t               size() const                            { return m_size; }

  bool                sync() const;
  bool                resize_file() const;

private:
  File*               m_file;

  off_t               m_position;
  off_t               m_size;
};

inline bool
StorageFile::is_valid_position(off_t p) const {
  return p >= m_position && p < m_position + m_size;
}

}

#endif
