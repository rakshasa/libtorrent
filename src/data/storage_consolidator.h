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

class StorageConsolidator : private std::vector<StorageFile> {
public:
  typedef std::vector<StorageFile> Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  StorageConsolidator() : m_size(0), m_chunksize(1 << 16) {}
  ~StorageConsolidator() { clear(); }

  // We take over ownership of 'file'.
  void                push_back(FileMeta* file, off_t size);

  // TODO: Rename this to something else.
  bool                resize_files();
  void                clear();

  void                sync();

  void                set_chunk_size(uint32_t size);

  size_t              get_files_size() const                  { return Base::size(); }
  off_t               get_bytes_size() const                  { return m_size; }

  // If this call returns false, the chunk might only be partially valid.
  bool                get_chunk(StorageChunk& chunk, uint32_t b, int prot);

  uint32_t            get_chunk_total() const                 { return (m_size + m_chunksize - 1) / m_chunksize; }
  uint32_t            get_chunk_size() const                  { return m_chunksize; }

  off_t               get_chunk_position(uint32_t c) const    { return c * (off_t)m_chunksize; }

private:
  MemoryChunk         get_chunk_part(iterator itr, off_t offset, uint32_t length, int prot);

  off_t               m_size;
  uint32_t            m_chunksize;
};

}

#endif

