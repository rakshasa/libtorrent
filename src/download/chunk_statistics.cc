// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include "torrent/exceptions.h"

#include "protocol/peer_chunks.h"

#include "chunk_statistics.h"

namespace torrent {

inline bool
ChunkStatistics::should_add(PeerChunks* pc) {
  return m_accounted < max_accounted;
}

void
ChunkStatistics::initialize(size_type s) {
  if (!empty())
    throw internal_error("ChunkStatistics::initialize(...) called on an initialized object.");

  base_type::resize(s);
}

void
ChunkStatistics::clear() {
  if (m_complete != 0)
    throw internal_error("ChunkStatistics::clear() m_complete != 0.");

  base_type::clear();
}

void
ChunkStatistics::received_connect(PeerChunks* pc) {
  if (pc->using_counter())
    throw internal_error("ChunkStatistics::received_connect(...) pc->using_counter() == true.");

  if (pc->bitfield()->is_all_set()) {
    pc->set_using_counter(true);
    m_complete++;

  } else if (!pc->bitfield()->is_all_unset() && should_add(pc)) {
    // There should be additional checks, so that we don't do this
    // when there's no need.
    pc->set_using_counter(true);
    m_accounted++;
    
    iterator itr = base_type::begin();

    // Use a bitfield iterator instead.
    for (Bitfield::size_type index = 0; index < pc->bitfield()->size_bits(); ++index, ++itr)
      *itr += pc->bitfield()->get(index);
  }
}

void
ChunkStatistics::received_disconnect(PeerChunks* pc) {

  // The 'using_counter' of complete peers is used, but not added to
  // 'm_accounted', so that we can safely disconnect peers right after
  // receiving the bitfield without calling 'received_connect'.
  if (!pc->using_counter())
    return;

  pc->set_using_counter(false);

  if (pc->bitfield()->is_all_set()) {
    m_complete--;

  } else {
    if (m_accounted == 0)
      throw internal_error("ChunkStatistics::received_disconnect(...) m_accounted == 0.");

    m_accounted--;

    iterator itr = base_type::begin();

    // Use a bitfield iterator instead.
    for (Bitfield::size_type index = 0; index < pc->bitfield()->size_bits(); ++index, ++itr)
      *itr -= pc->bitfield()->get(index);
  }
}

void
ChunkStatistics::received_have_chunk(PeerChunks* pc, uint32_t index, uint32_t length) {
  // When the bitfield is empty, it is very cheap to add the peer to
  // the statistics. It needs to be done here else we need to check if
  // a connection has sent any messages, else it might send a bitfield.
  if (pc->bitfield()->is_all_unset() && should_add(pc)) {

    if (pc->using_counter())
      throw internal_error("ChunkStatistics::received_have_chunk(...) pc->using_counter() == true.");

    pc->set_using_counter(true);
    m_accounted++;
  }

  pc->bitfield()->set(index);
  pc->peer_rate()->insert(length);
  
  if (pc->using_counter()) {

    base_type::operator[](index)++;

    // The below code should not cause useless work to be done in case
    // of immediate disconnect.
    if (pc->bitfield()->is_all_set()) {
      if (m_accounted == 0)
	throw internal_error("ChunkStatistics::received_disconnect(...) m_accounted == 0.");
      
      m_complete++;
      m_accounted--;
      
      for (iterator itr = base_type::begin(), last = base_type::end(); itr != last; ++itr)
	*itr -= 1;
    }

  } else {

    if (pc->bitfield()->is_all_set()) {
      pc->set_using_counter(true);
      m_complete++;
    }
  }
}

}
