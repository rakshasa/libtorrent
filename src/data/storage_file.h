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

#include "file.h"

namespace torrent {

class File;

class StorageFile {
public:
  StorageFile() : m_file(NULL), m_position(0), m_size(0) {}
  StorageFile(File* f, uint64_t p, uint64_t s) : m_file(f), m_position(p), m_size(s) {}

  bool        is_valid()        { return m_file; }

  File*       file()            { return m_file; }
  uint64_t    position()        { return m_position; }
  uint64_t    size()            { return m_size; }

  File const* c_file() const    { return m_file; }

  bool        sync();

private:
  File* m_file;
  uint64_t m_position;
  uint64_t m_size;
};

}

#endif
