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

#ifndef LIBTORRENT_STORAGE_CONSOLIDATOR_H
#define LIBTORRENT_STORAGE_CONSOLIDATOR_H

#include "storage_file.h"
#include "storage_chunk.h"

namespace torrent {

class StorageConsolidator {
public:
  typedef std::vector<StorageFile> FileList;

  StorageConsolidator() : m_size(0), m_chunksize(1 << 16) {}
  ~StorageConsolidator();

  // We take over ownership of 'file'.
  void            add_file(File* file, uint64_t size);

  bool            resize();
  bool            sync();
  void            close();

  void            set_chunksize(uint32_t size);

  uint64_t        get_size()                     { return m_size; }
  uint32_t        get_chunk_total()              { return (m_size + m_chunksize - 1) / m_chunksize; }
  uint32_t        get_chunk_size()               { return m_chunksize; }

  bool            get_chunk(StorageChunk& chunk, uint32_t b, bool wr = false, bool rd = true);

  FileList&       get_files()                    { return m_files; }

private:
  uint64_t        m_size;
  uint32_t        m_chunksize;
  
  FileList        m_files;
};

}

#endif

