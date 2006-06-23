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

#include "config.h"

#include <functional>

#include "torrent/exceptions.h"

#include "chunk.h"
#include "chunk_iterator.h"

namespace torrent {

bool
Chunk::is_all_valid() const {
  return !empty() && std::find_if(begin(), end(), std::not1(std::mem_fun_ref(&ChunkPart::is_valid))) == end();
}

void
Chunk::clear() {
  std::for_each(begin(), end(), std::mem_fun_ref(&ChunkPart::clear));

  m_size = 0;
  m_prot = 0;
  base_type::clear();
}

Chunk::iterator
Chunk::at_position(uint32_t pos) {
  if (pos >= m_size)
    throw internal_error("Chunk::at_position(...) tried to get Chunk position out of range.");

  iterator itr = std::find_if(begin(), end(), std::bind2nd(std::mem_fun_ref(&ChunkPart::is_contained), pos));

  if (itr == end())
    throw internal_error("Chunk::at_position(...) might be mangled, at_position failed horribly");

  if (itr->size() == 0)
    throw internal_error("Chunk::at_position(...) tried to return a node with length 0");

  return itr;
}

Chunk::data_type
Chunk::at_memory(uint32_t offset, iterator part) {
  if (part == end())
    throw internal_error("Chunk::at_memory(...) reached end.");

  if (!part->chunk().is_valid())
    throw internal_error("Chunk::at_memory(...) chunk part isn't valid.");

  if (offset < part->position() || offset >= part->position() + part->size())
    throw internal_error("Chunk::at_memory(...) out of range.");

  offset -= part->position();

  return data_type(part->chunk().begin() + offset, part->size() - offset);
}

// Each add calls vector's reserve adding 1. This should keep
// the size of the vector at exactly what we need. Though it
// will require a few more cycles, it won't matter as we only
// rarely have more than 1 or 2 nodes.
void
Chunk::push_back(const MemoryChunk& c) {
  if (empty())
    m_prot = c.get_prot();
  else
    m_prot &= c.get_prot();

  //base_type::reserve(base_type::size() + 1);
  base_type::insert(end(), ChunkPart(c, m_size));

  m_size += c.size();
}

uint32_t
Chunk::incore_length(uint32_t pos) {
  uint32_t lengthIncore = 0;
  iterator itr = at_position(pos);

  if (itr == end())
    throw internal_error("Chunk::incore_length(...) at end()");

  do {
    uint32_t length = itr->incore_length(pos);

    pos += length;
    lengthIncore += length;

  } while (pos == itr->position() + itr->size() && ++itr != end());

  return lengthIncore;
}

bool
Chunk::sync(int flags) {
  bool success = true;

  for (iterator itr = begin(), last = end(); itr != last; ++itr)
    if (!itr->chunk().sync(0, itr->chunk().size(), flags))
      success = false;

  return success;
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::to_buffer(void* buffer, uint32_t position, uint32_t length) {
  if (position + length > m_size)
    throw internal_error("Chunk::to_buffer(...) position + length > m_size.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  do {
    data = itr.data();
    std::memcpy(buffer, data.first, data.second);

    buffer = static_cast<char*>(buffer) + data.second;

  } while (itr.used(data.second));
  
  return true;
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::from_buffer(const void* buffer, uint32_t position, uint32_t length) {
  if (position + length > m_size)
    throw internal_error("Chunk::from_buffer(...) position + length > m_size.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  do {
    data = itr.data();
    std::memcpy(data.first, buffer, data.second);

    buffer = static_cast<const char*>(buffer) + data.second;

  } while (itr.used(data.second));
  
  return true;
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::compare_buffer(const void* buffer, uint32_t position, uint32_t length) {
  if (position + length > m_size)
    throw internal_error("Chunk::compare_buffer(...) position + length > m_size.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  do {
    data = itr.data();

    if (std::memcmp(data.first, buffer, data.second) != 0)
      return false;

    buffer = static_cast<const char*>(buffer) + data.second;

  } while (itr.used(data.second));
  
  return true;
}

}
