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

#include "torrent/exceptions.h"
#include "data/chunk_list.h"
#include "hash_torrent.h"
#include "hash_queue.h"

namespace torrent {

HashTorrent::HashTorrent(ChunkList* c) :
  m_position(0),
  m_outstanding(-1),
  m_chunkList(c),
  m_queue(NULL) {
}

void
HashTorrent::start() {
  if (is_checking() ||
      m_position > 0 ||
      m_queue == NULL ||
      m_chunkList == NULL ||
      m_chunkList->empty())
    throw internal_error("HashTorrent::start() call failed.");

  m_outstanding = 0;
  queue();
}

void
HashTorrent::clear() {
//   if (is_checking())
//     m_queue->remove(m_id);

  m_outstanding = -1;
  m_position = 0;
}

bool
HashTorrent::is_checked() {
  return !m_chunkList->empty() && m_position == m_chunkList->size();
}

void
HashTorrent::receive_chunkdone() {
  if (m_outstanding == -1)
//     throw internal_error("HashTorrent::receive_chunkdone() m_outstanding < 0.");
    return;

  // m_signalChunk will always point to
  // DownloadMain::receive_hash_done, so it will take care of cleanup.
  //
  // Make sure we call chunkdone before torrentDone has a chance to
  // trigger.
//   m_slotChunkDone(handle, hash);
  m_outstanding--;

  // Don't add more when we've stopped. Use some better condition than
  // m_outstanding. This code is ugly... needs a refactoring, a
  // seperate flag for active and allow pause or clearing the state.
  if (m_outstanding >= 0)
    queue();
}

void
HashTorrent::queue() {
  if (!is_checking())
    throw internal_error("HashTorrent::queue() called but it's not running.");

  while (m_position < m_chunkList->size()) {
    if (m_outstanding >= 30)
      return;

    // Not very efficient, but this is seldomly done.
    Ranges::iterator itr = m_ranges.find(m_position);

    if (itr == m_ranges.end()) {
      m_position = m_chunkList->size();
      break;
    } else if (m_position < itr->first) {
      m_position = itr->first;
    }

    ChunkHandle handle = m_chunkList->get(m_position++, false);

    if (!handle.is_valid())
      continue;

    // If the error number is not valid, then we've just encountered a
    // file that hasn't be created/resized. Which means we ignore it
    // when doing initial hashing.
    if (handle.error_number().is_valid()) {
      m_slotInitialHash();
      m_slotStorageError("Hash checker was unable to map chunk: " + std::string(handle.error_number().c_str()));

      return;
    }
    
    m_slotCheckChunk(handle);
    m_outstanding++;
  }

  if (m_outstanding == 0) {
    m_outstanding = -1;
    m_slotInitialHash();
  }
}

}
