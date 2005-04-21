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

#include "file_meta.h"
#include "storage_file.h"

namespace torrent {

void
StorageFile::clear() {
  delete m_meta;

  m_meta = NULL;
  m_position = m_size = 0;
}

bool
StorageFile::sync() const {
  if (!m_meta->prepare(MemoryChunk::prot_read))
    return false;

  off_t pos = 0;

  while (pos != m_size) {
    uint32_t length = std::min(m_size - pos, (off_t)(128 << 20));

    MemoryChunk c = m_meta->get_file().get_chunk(pos, length, MemoryChunk::prot_read, MemoryChunk::map_shared);

    if (!c.is_valid())
      return false;

    c.sync(0, length, MemoryChunk::sync_async);
    c.unmap();

    pos += length;
  }

  return true;
}

bool
StorageFile::resize_file() const {
  if (!m_meta->prepare(MemoryChunk::prot_read))
    return false;

  if (m_size == m_meta->get_file().get_size())
    return true;
  else
    return
      m_meta->prepare(MemoryChunk::prot_read | MemoryChunk::prot_write) &&
      m_meta->get_file().set_size(m_size);
}

}
