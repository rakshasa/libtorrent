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

#include "torrent/exceptions.h"
#include "storage.h"
#include "hash_torrent.h"
#include "hash_queue.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

HashTorrent::HashTorrent(const std::string& id, Storage* s) :
  m_id(id),
  m_position(0),
  m_outstanding(0),
  m_storage(s),
  m_queue(NULL) {
}

void
HashTorrent::start() {
  if (m_queue == NULL || m_storage == NULL)
    throw internal_error("HashTorrent::start() called on an object with invalid m_queue or m_storage");

  if (is_checking() || m_position == m_storage->get_chunk_total())
    return;

  queue();
}

void
HashTorrent::stop() {
  // TODO: Don't allow stops atm, just clean up.
  m_queue->remove(m_id);
  m_outstanding = 0;
}
  
void
HashTorrent::receive_chunkdone(Chunk c, std::string hash) {
  // Make sure we call chunkdone before torrentDone has a chance to
  // trigger.
  m_signalChunk(c, hash);
  m_outstanding--;

  queue();
}

void
HashTorrent::queue() {
  while (m_position < m_storage->get_chunk_total()) {
    if (m_outstanding >= 30)
      return;

    // Not very efficient, but this is seldomly done.
    Ranges::iterator itr = m_ranges.find(m_position);

    if (itr == m_ranges.end())
      break;
    else if (m_position < itr->first)
      m_position = itr->first;

    Chunk c = m_storage->get_chunk(m_position++, MemoryChunk::prot_read);

    if (!c.is_valid() || !c->is_valid())
      continue;

    m_queue->push_back(c, sigc::mem_fun(*this, &HashTorrent::receive_chunkdone), m_id);
    m_outstanding++;
  }

  if (!m_outstanding)
    m_signalTorrent();
}

}
