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

#include <algorithm>
#include <functional>

#include "torrent/exceptions.h"
#include "file.h"
#include "storage_consolidator.h"

namespace torrent {

void
StorageConsolidator::push_back(File* file, off_t size) {
  if (sizeof(off_t) != 8)
    throw internal_error("sizeof(off_t) != 8");

  if (size + m_size < m_size)
    throw internal_error("Sum of files added to StorageConsolidator overflowed 64bit");

  if (file == NULL)
    throw internal_error("StorageConsolidator::add_file received a File NULL pointer");

  Base::push_back(StorageFile(file, m_size, size));
  m_size += size;
}

bool
StorageConsolidator::resize_files() {
  return std::find_if(begin(), end(), std::not1(std::mem_fun_ref(&StorageFile::resize_file))) == end();
}
					   
void
StorageConsolidator::clear() {
  std::for_each(begin(), end(), std::mem_fun_ref(&StorageFile::clear));

  Base::clear();
  m_size = 0;
}

void
StorageConsolidator::sync() {
  std::for_each(begin(), end(), std::mem_fun_ref(&StorageFile::sync));
}

void
StorageConsolidator::set_chunk_size(uint32_t size) {
  if (size == 0)
    throw internal_error("Tried to set StorageConsolidator's chunksize to zero");

  m_chunksize = size;
}

bool
StorageConsolidator::get_chunk(StorageChunk& chunk, uint32_t b, int prot) {
  MemoryChunk mc;

  off_t first = get_chunk_position(b);
  off_t last = std::min(get_chunk_position(b + 1), m_size);

  if (first >= m_size)
    throw internal_error("Tried to access chunk out of range in StorageConsolidator");

  iterator itr = std::find_if(begin(), end(), std::bind2nd(std::mem_fun_ref(&StorageFile::is_valid_position), first));

  while (first != last) {
    if (itr == end())
      throw internal_error("StorageConsolidator could not find a valid file for chunk");

    if (!(mc = get_chunk_part(itr, first, last - first, prot)).is_valid())
      return false;

    chunk.push_back(mc);

    first += mc.size();
    ++itr;
  }

  if (chunk.get_size() != last - get_chunk_position(b))
    throw internal_error("StorageConsolidator::get_chunk didn't get a chunk with the correct size");

  return true;
}

MemoryChunk
StorageConsolidator::get_chunk_part(iterator itr, off_t offset, uint32_t length, int prot) {
  offset -= itr->get_position();
  length = std::min<off_t>(length, itr->get_size() - offset);

  if (offset < 0)
    throw internal_error("StorageConsolidator::get_chunk_part(...) caught a negative offset");

  if (length == 0)
    throw internal_error("StorageConsolidator::get_chunk_part(...) caught a piece with 0 lenght");

  if (length > m_chunksize)
    throw internal_error("StorageConsolidator::get_chunk_part(...) caught an excessively large piece");

  return itr->get_file()->get_chunk(offset, length, prot, MemoryChunk::map_shared);
}

}

