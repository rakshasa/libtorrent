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

#include <cstdlib>

#include "protocol/peer_chunks.h"
#include "torrent/exceptions.h"

#include "chunk_selector.h"

namespace torrent {

void
ChunkSelector::initialize(BitField* bf) {
  m_bitfield = *bf;
  m_position = invalid_chunk;
}

void
ChunkSelector::cleanup() {
  m_bitfield = BitField();
}

void
ChunkSelector::update_priorities() {
  m_position = std::rand() % size();

  // Find the next wanted piece.
}

uint32_t
ChunkSelector::find(PeerChunks* peerChunks, bool highPriority) {
  if (m_position == invalid_chunk)
    return m_position;

  uint32_t position;

  if ((position = search(peerChunks, &m_highPriority, m_position, m_bitfield.size_bits())) == invalid_chunk &&
      (position = search(peerChunks, &m_highPriority, 0, m_position)) == invalid_chunk &&
      (position = search(peerChunks, &m_normalPriority, m_position, m_bitfield.size_bits())) == invalid_chunk &&
      (position = search(peerChunks, &m_normalPriority, 0, m_position)) == invalid_chunk)
    return invalid_chunk;

  if (m_bitfield.get(position))
    throw internal_error("ChunkSelector::find(...) tried to return an index that is already set.");

  return position;
}

void
ChunkSelector::select_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::select_index(...) index out of range.");

  if (m_bitfield.get(index))
    throw internal_error("ChunkSelector::select_index(...) index already set.");

  m_bitfield.set(index, true);
}

void
ChunkSelector::deselect_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::deselect_index(...) index out of range.");

  if (!m_bitfield.get(index))
    throw internal_error("ChunkSelector::deselect_index(...) index already unset.");

  m_bitfield.set(index, false);
}

void
ChunkSelector::insert_peer_chunks(PeerChunks* peerChunks) {
}

void
ChunkSelector::erase_peer_chunks(PeerChunks* peerChunks) {
}

inline uint32_t
ChunkSelector::search(PeerChunks* peerChunks, PriorityRanges* ranges, uint32_t first, uint32_t last) {
  uint32_t position;
  PriorityRanges::iterator itr = ranges->find(m_position);

  while (itr != ranges->end() && itr->first < last) {

    if ((position = search_range(peerChunks, std::max(first, itr->first), std::min(last, itr->second))) != invalid_chunk)
      return position;
    
    ++itr;    
  }

  return invalid_chunk;
}

// Could propably add another argument for max seen or something, this
// would be used to find better chunks to request.
inline uint32_t
ChunkSelector::search_range(PeerChunks* peerChunks, uint32_t first, uint32_t last) {
  if (first >= last || last > size())
    throw internal_error("ChunkSelector::find_range(...) received an invalid range.");

  BitFieldExt::const_iterator local  = m_bitfield.begin() + first / 8;
  BitFieldExt::const_iterator source = peerChunks->bitfield()->begin() + first / 8;

  // Unset any bits before 'first'.
  BitFieldExt::data_t wanted = (*source & ~*local) & (~(BitFieldExt::data_t)0 >> (first % 8));

  while (wanted == 0) {
    ++local;
    ++source;

    if (m_bitfield.position(local) >= last)
      return invalid_chunk;

    wanted = (*source & ~*local);
  }
  
  uint32_t position = m_bitfield.position(local) + search_byte(wanted);

  if (position < last)
    return position;
  else
    return invalid_chunk;
}

inline uint32_t
ChunkSelector::search_byte(uint8_t wanted) {
  uint32_t i = 0;

  while ((wanted & ((BitFieldExt::data_t)1 << (7 - i))) == 0)
    i++;
  
  return i;
}

}
