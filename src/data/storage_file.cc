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

#include "config.h"

#include "storage_file.h"

namespace torrent {

bool
StorageFile::sync() {
  FileChunk f;
  uint64_t pos = 0;

  while (pos != m_size) {
    uint32_t length = std::min<uint64_t>(m_size - pos, (128 << 20));

    if (!m_file->get_chunk(f, pos, length, true))
      return false;

    f.sync(0, length, FileChunk::sync_async);
    pos += length;
  }

  return true;
}

}
