// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include <algorithm>
#include <csetjmp>
#include <csignal>
#include <cstring>
#include <functional>

#include "torrent/exceptions.h"

#include "chunk.h"
#include "chunk_iterator.h"

namespace {
jmp_buf jmp_disk_full;

void
bus_handler(int, siginfo_t* si, void*) {
  if (si && si->si_code == BUS_ADRERR)
    longjmp(jmp_disk_full, 1);
}
} // namespace

namespace torrent {

bool
Chunk::is_all_valid() const {
  return !empty() && std::all_of(begin(), end(), std::mem_fn(&ChunkPart::is_valid));
}

void
Chunk::clear() {
  std::for_each(begin(), end(), std::mem_fn(&ChunkPart::clear));

  m_chunkSize = 0;
  m_prot = ~0;
  base_type::clear();
}

// Each add calls vector's reserve adding 1. This should keep
// the size of the vector at exactly what we need. Though it
// will require a few more cycles, it won't matter as we only
// rarely have more than 1 or 2 nodes.
//
// If the user knows how many chunk parts he is going to add, then he
// may call reserve prior to this.
void
Chunk::push_back(value_type::mapped_type mapped, const MemoryChunk& c) {
  m_prot &= c.get_prot();

  // Gcc starts the reserved size at 1 for the first insert, so we
  // won't be wasting any space in the general case.
  base_type::insert(end(), ChunkPart(mapped, c, m_chunkSize));

  m_chunkSize += c.size();
}

Chunk::iterator
Chunk::at_position(uint32_t pos) {
  if (pos >= m_chunkSize)
    throw internal_error("Chunk::at_position(...) tried to get Chunk position out of range.");

  auto itr = std::find_if(begin(), end(), [pos](const auto& chunk) { return chunk.is_contained(pos); });
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

bool
Chunk::is_incore(uint32_t pos, uint32_t length) {
  auto itr = at_position(pos);

  if (itr == end())
    throw internal_error("Chunk::incore_length(...) at end()");

  uint32_t last = pos + std::min(length, chunk_size() - pos);

  while (itr->is_incore(pos, last - pos)) {
    if (++itr == end() || itr->position() >= last)
      return true;

    pos = itr->position();
  }

  return false;
}

// TODO: Buggy, hitting internal_error. Likely need to fix
// ChunkPart::incore_length's length align.
uint32_t
Chunk::incore_length(uint32_t pos, uint32_t length) {
  uint32_t result = 0;
  auto itr = at_position(pos);

  if (itr == end())
    throw internal_error("Chunk::incore_length(...) at end()");

  length = std::min(length, chunk_size() - pos);

  do {
    uint32_t incore_len = itr->incore_length(pos, length);

    if (incore_len > length)
      throw internal_error("Chunk::incore_length(...) incore_len > length.");

    pos += incore_len;
    length -= incore_len;
    result += incore_len;

  } while (pos == itr->position() + itr->size() && ++itr != end());

  return result;
}

bool
Chunk::sync(int flags) {
  bool success = true;

  for (auto& c : *this)
    if (!c.chunk().sync(0, c.chunk().size(), flags))
      success = false;

  return success;
}

void
Chunk::preload(uint32_t position, uint32_t length, bool useAdvise) {
  if (position >= m_chunkSize)
    throw internal_error("Chunk::preload(...) position > m_chunkSize.");

  if (length == 0)
    return;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + std::min(length, m_chunkSize - position));

  do {
    data = itr.data();

    // Don't do preloading for zero-length chunks.

    if (useAdvise) {
      itr.memory_chunk()->advise(itr.memory_chunk_first(), data.second, MemoryChunk::advice_willneed);

    } else {
      for (char* first = static_cast<char*>(data.first), *last = static_cast<char*>(data.first) + data.second; first < last; first += 4096)
        [[maybe_unused]] volatile char touchChunk = *static_cast<char*>(data.first);

      // Make sure we touch the last page in the range.
      [[maybe_unused]] volatile char ouchChunk = *(static_cast<char*>(data.first) + data.second - 1);
    }

  } while (itr.next());
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::to_buffer(void* buffer, uint32_t position, uint32_t length) {
  if (position + length > m_chunkSize)
    throw internal_error("Chunk::to_buffer(...) position + length > m_chunkSize.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  do {
    data = itr.data();
    std::memcpy(buffer, data.first, data.second);

    buffer = static_cast<char*>(buffer) + data.second;
  } while (itr.next());
  
  return true;
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::from_buffer(const void* buffer, uint32_t position, uint32_t length) {
  struct sigaction sa{}, oldact;
  sa.sa_sigaction = &bus_handler;
  sa.sa_flags = SA_SIGINFO;
  sigfillset(&sa.sa_mask);
  sigaction(SIGBUS, &sa, &oldact);

  if (position + length > m_chunkSize)
    throw internal_error("Chunk::from_buffer(...) position + length > m_chunkSize.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  if (setjmp(jmp_disk_full) == 0) {
      do {
        data = itr.data();
        std::memcpy(data.first, buffer, data.second);

        buffer = static_cast<const char*>(buffer) + data.second;
      } while (itr.next());
  } else {
      throw storage_error("no space left on disk");
  }

  sigaction(SIGBUS, &oldact, NULL);
  
  return true;
}

// Consider using uint32_t returning first mismatch or length if
// matching.
bool
Chunk::compare_buffer(const void* buffer, uint32_t position, uint32_t length) {
  if (position + length > m_chunkSize)
    throw internal_error("Chunk::compare_buffer(...) position + length > m_chunkSize.");

  if (length == 0)
    return true;

  Chunk::data_type data;
  ChunkIterator itr(this, position, position + length);

  do {
    data = itr.data();

    if (std::memcmp(data.first, buffer, data.second) != 0)
      return false;

    buffer = static_cast<const char*>(buffer) + data.second;
  } while (itr.next());
  
  return true;
}

} // namespace torrent
