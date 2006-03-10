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

#include <algorithm>
#include <cstdlib>
#include <rak/functional.h>

#include "protocol/peer_chunks.h"
#include "torrent/exceptions.h"

#include "chunk_selector.h"

namespace torrent {

void
ChunkSelector::initialize(BitField* bf) {
  m_position = invalid_chunk;

  m_bitfield = BitField(bf->size_bits());

  std::transform(bf->begin(), bf->end(), m_bitfield.begin(), rak::invert<BitField::data_t>());
  m_bitfield.cleanup();
}

void
ChunkSelector::cleanup() {
  m_bitfield = BitField();
}

// Consider if ChunksSelector::not_using_index(...) needs to be
// modified.
void
ChunkSelector::update_priorities() {
  if (empty())
    return;

  if (m_position == invalid_chunk)
    m_position = std::rand() % size();

  advance_position();
}

uint32_t
ChunkSelector::find(PeerChunks* peerChunks, __UNUSED bool highPriority) {
  if (m_position == invalid_chunk)
    return m_position;

  uint32_t position;

  if ((position = search(peerChunks->bitfield()->base(), &m_highPriority, m_position, size())) == invalid_chunk &&
      (position = search(peerChunks->bitfield()->base(), &m_highPriority, 0, m_position)) == invalid_chunk &&
      (position = search(peerChunks->bitfield()->base(), &m_normalPriority, m_position, size())) == invalid_chunk &&
      (position = search(peerChunks->bitfield()->base(), &m_normalPriority, 0, m_position)) == invalid_chunk)
    return invalid_chunk;

  if (!m_bitfield.get(position))
    throw internal_error("ChunkSelector::find(...) tried to return an index that is already set.");

  return position;
}

bool
ChunkSelector::is_wanted(uint32_t index) const {
  return m_bitfield.get(index) && (m_normalPriority.has(index) || m_highPriority.has(index));
}

void
ChunkSelector::using_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::select_index(...) index out of range.");

  if (!m_bitfield.get(index))
    throw internal_error("ChunkSelector::select_index(...) index already set.");

  m_bitfield.set(index, false);

  // We always know 'm_position' points to a wanted chunk. If it
  // changes, we need to move m_position to the next one.
  if (index == m_position)
    advance_position();
}

void
ChunkSelector::not_using_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::deselect_index(...) index out of range.");

  if (m_bitfield.get(index))
    throw internal_error("ChunkSelector::deselect_index(...) index already unset.");

  m_bitfield.set(index, true);

  // This will make sure that if we enable new chunks, it will work
  // when 'index == invalid_chunk'.
  update_priorities();
}

void
ChunkSelector::insert_peer_chunks(__UNUSED PeerChunks* peerChunks) {
}

void
ChunkSelector::erase_peer_chunks(__UNUSED PeerChunks* peerChunks) {
}

inline uint32_t
ChunkSelector::search(const BitField* bf, PriorityRanges* ranges, uint32_t first, uint32_t last) {
  uint32_t position;
  PriorityRanges::iterator itr = ranges->find(first);

  while (itr != ranges->end() && itr->first < last) {

    if ((position = search_range(bf, std::max(first, itr->first), std::min(last, itr->second))) != invalid_chunk)
      return position;
    
    ++itr;    
  }

  return invalid_chunk;
}

// Could propably add another argument for max seen or something, this
// would be used to find better chunks to request.
inline uint32_t
ChunkSelector::search_range(const BitField* bf, uint32_t first, uint32_t last) {
  if (first >= last || last > size())
    throw internal_error("ChunkSelector::search_range(...) received an invalid range.");

  BitField::const_iterator local  = m_bitfield.begin() + first / 8;
  BitField::const_iterator source = bf->begin() + first / 8;

  // Unset any bits before 'first'.
  BitField::data_t wanted = (*source & *local) & (0xff >> (first % 8));

  while (wanted == 0) {
    ++local;
    ++source;

    if (m_bitfield.position(local) >= last)
      return invalid_chunk;

    wanted = (*source & *local);
  }
  
  uint32_t position = m_bitfield.position(local) + search_byte(wanted);

  if (position < first)
    throw internal_error("ChunkSelector::search_range(...) position < first.");

  if (position < last)
    return position;
  else
    return invalid_chunk;
}

inline uint32_t
ChunkSelector::search_byte(uint8_t wanted) {
  uint32_t i = 0;

  while ((wanted & ((BitField::data_t)1 << (7 - i))) == 0)
    i++;
  
  return i;
}

void
ChunkSelector::advance_position() {
  int position = m_position;

  // Find the next wanted piece.
  if ((m_position = search(&m_bitfield, &m_highPriority, position, size())) == invalid_chunk &&
      (m_position = search(&m_bitfield, &m_highPriority, 0, position)) == invalid_chunk &&
      (m_position = search(&m_bitfield, &m_normalPriority, position, size())) == invalid_chunk &&
      (m_position = search(&m_bitfield, &m_normalPriority, 0, position)) == invalid_chunk)
    ;
}

}
