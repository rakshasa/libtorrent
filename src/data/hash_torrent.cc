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

#include "data/chunk_list.h"
#include "torrent/exceptions.h"

#include "hash_torrent.h"
#include "hash_queue.h"
#include "globals.h"

namespace torrent {

HashTorrent::HashTorrent(ChunkList* c) :
  m_position(0),
  m_outstanding(-1),
  m_errno(0),

  m_chunkList(c) {
}

bool
HashTorrent::start(bool tryQuick) {
  if (m_position == m_chunkList->size())
    return true;

  if (!is_checking()) {
    if (m_position > 0 || m_chunkList->empty())
      throw internal_error("HashTorrent::start() call failed.");

    m_outstanding = 0;
  }

  // This doesn't really handle paused hashing properly... Do we set
  // m_outstanding to -1 when stopping?

  queue(tryQuick);
  return m_position == m_chunkList->size();
}

void
HashTorrent::clear() {
  m_outstanding = -1;
  m_position = 0;
  m_errno = 0;
}

bool
HashTorrent::is_checked() {
  // When closed the chunk list is empty. Position can be equal to
  // chunk list for a short while as we have outstanding chunks, so
  // check the latter.
  return !m_chunkList->empty() && m_position == m_chunkList->size() && m_outstanding == -1;
}

// After all chunks are checked it won't show as is_checked until
// after this function is called. This allows for the hash done signal
// to be delayed.
void
HashTorrent::confirm_checked() {
  if (m_outstanding != 0)
    throw internal_error("HashTorrent::confirm_checked() m_outstanding != 0.");

  m_outstanding = -1;
}

void
HashTorrent::receive_chunkdone() {
  if (m_outstanding == -1)
    throw internal_error("HashTorrent::receive_chunkdone() m_outstanding < 0.");

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
    queue(false);
}

// Mark unsuccessful checks so that if we have just stopped the
// hash checker it will ensure those pieces get rechecked upon
// restart.
void
HashTorrent::receive_chunk_cleared(uint32_t index) {
  if (m_outstanding <= 0)
    throw internal_error("HashTorrent::receive_chunk_cleared() m_outstanding < 0.");
  
  if (m_ranges.has(index))
    throw internal_error("HashTorrent::receive_chunk_cleared() m_ranges.has(index).");

  m_outstanding--;
  m_ranges.insert(index, index + 1);
}

void
HashTorrent::queue(bool quick) {
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

    // Need to do increment later if we're going to support resume
    // hashing a quick hashed torrent.
    ChunkHandle handle = m_chunkList->get(m_position, false);

    if (quick) {
      // We're not actually interested in doing any hashing, so just
      // skip what we know is not possible to hash.
      //
      // If the file does not exist then no valid error number is
      // returned.

      if (m_outstanding != 0)
        throw internal_error("HashTorrent::queue() quick hashing but m_outstanding != 0.");

      if (handle.is_valid())
        return m_chunkList->release(&handle);
      
      if (handle.error_number().is_valid())
        return;

      m_position++;
      continue;

    } else {
      // If the error number is not valid, then we've just encountered a
      // file that hasn't be created/resized. Which means we ignore it
      // when doing initial hashing.
      if (handle.error_number().is_valid()) {
        if (handle.is_valid())
          throw internal_error("HashTorrent::queue() error, but handle.is_valid().");

        // We wait for all the outstanding chunks to be checked before
        // borking completely, else low-memory devices might not be able
        // to finish the hash check.
        if (m_outstanding != 0)
          return;

        // The rest of the outstanding chunks get ignored by
        // DownloadWrapper::receive_hash_done. Obsolete.
        clear();

        m_errno = handle.error_number().value();

        rak::priority_queue_erase(&taskScheduler, &m_delayChecked);
        rak::priority_queue_insert(&taskScheduler, &m_delayChecked, cachedTime);
        return;
      }

      m_position++;

      // Missing file, skip the hash check.
      if (!handle.is_valid())
        continue;

      m_slotCheck(handle);
      m_outstanding++;
    }
  }

  if (m_outstanding == 0) {
    // Erase the scheduled item just to make sure that if hashing is
    // started again during the delay it won't cause an exception.
    rak::priority_queue_erase(&taskScheduler, &m_delayChecked);
    rak::priority_queue_insert(&taskScheduler, &m_delayChecked, cachedTime);
  }
}

}
