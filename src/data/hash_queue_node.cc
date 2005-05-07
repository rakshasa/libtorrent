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

#include "hash_queue_node.h"

namespace torrent {

bool
HashQueueNode::perform(bool force) {
  if (!m_chunk->perform(m_chunk->remaining(), force))
    return false;

  m_slotDone(m_chunk->get_chunk(), m_chunk->get_hash());

  return true;
}

uint32_t
HashQueueNode::call_willneed() {
  if (!m_willneed) {
    m_willneed = true;
    m_chunk->advise_willneed(m_chunk->remaining());
  }

  return m_chunk->remaining();
}

}
