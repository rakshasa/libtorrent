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

#define __STDC_FORMAT_MACROS

#include "data/chunk_list.h"
#include "torrent/exceptions.h"
#include "torrent/data/download_data.h"
#include "torrent/utils/log.h"

#include "hash_torrent.h"
#include "hash_queue.h"
#include "globals.h"

#define LT_LOG_THIS(log_level, log_fmt, ...)                            \
  lt_log_print_data(LOG_STORAGE_##log_level, m_chunk_list->data(), "hash_torrent", log_fmt, __VA_ARGS__);

namespace torrent {

HashTorrent::HashTorrent(ChunkList* c) :
  m_position(0),
  m_outstanding(-1),
  m_errno(0),

  m_chunk_list(c) {
}

bool
HashTorrent::start(bool try_quick) {
  LT_LOG_THIS(INFO, "Start: position:%u size:%" PRIu64 " try_quick:%u.",
              m_position, m_chunk_list->size(), try_quick);

  if (m_position == m_chunk_list->size())
    return true;

  if (m_position > 0 || m_chunk_list->empty())
    throw internal_error("HashTorrent::start() call failed.");

  m_outstanding = 0;

  queue(try_quick);
  return m_position == m_chunk_list->size();
}

void
HashTorrent::clear() {
  LT_LOG_THIS(INFO, "Clear.", 0);

  m_outstanding = -1;
  m_position = 0;
  m_errno = 0;

  // Correct?
  rak::priority_queue_erase(&taskScheduler, &m_delayChecked);
}

bool
HashTorrent::is_checked() {
  // When closed the chunk list is empty. Position can be equal to
  // chunk list for a short while as we have outstanding chunks, so
  // check the latter.
  return !m_chunk_list->empty() && m_position == m_chunk_list->size() && m_outstanding == -1;
}

// After all chunks are checked it won't show as is_checked until
// after this function is called. This allows for the hash done signal
// to be delayed.
void
HashTorrent::confirm_checked() {
  LT_LOG_THIS(INFO, "Confirm checked.", 0);

  if (m_outstanding != 0)
    throw internal_error("HashTorrent::confirm_checked() m_outstanding != 0.");

  m_outstanding = -1;
}

void
HashTorrent::receive_chunkdone(uint32_t index) {
  LT_LOG_THIS(DEBUG, "Received chunk done: index:%" PRIu32 ".", index);

  if (m_outstanding <= 0)
    throw internal_error("HashTorrent::receive_chunkdone() m_outstanding <= 0.");

  // m_signalChunk will always point to
  // DownloadMain::receive_hash_done, so it will take care of cleanup.
  //
  // Make sure we call chunkdone before torrentDone has a chance to
  // trigger.
  m_outstanding--;

  queue(false);
}

// Mark unsuccessful checks so that if we have just stopped the
// hash checker it will ensure those pieces get rechecked upon
// restart.
void
HashTorrent::receive_chunk_cleared(uint32_t index) {
  LT_LOG_THIS(DEBUG, "Received chunk cleared: index:%" PRIu32 ".", index);

  if (m_outstanding <= 0)
    throw internal_error("HashTorrent::receive_chunk_cleared() m_outstanding < 0.");
  
  if (m_ranges.has(index))
    throw internal_error("HashTorrent::receive_chunk_cleared() m_ranges.has(index).");

  m_outstanding--;
  m_ranges.insert(index, index + 1);
}

void
HashTorrent::queue(bool quick) {
  LT_LOG_THIS(DEBUG, "Queue: position:%u outstanding:%i try_quick:%u.", m_position, m_outstanding, quick);

  if (!is_checking())
    throw internal_error("HashTorrent::queue() called but it's not running.");

  while (m_position < m_chunk_list->size()) {
    if (m_outstanding > 10 && m_outstanding * m_chunk_list->chunk_size() > (128 << 20))
      return;

    // Not very efficient, but this is seldomly done.
    Ranges::iterator itr = m_ranges.find(m_position);

    if (itr == m_ranges.end()) {
      m_position = m_chunk_list->size();
      break;
    } else if (m_position < itr->first) {
      m_position = itr->first;
    }

    // Need to do increment later if we're going to support resume
    // hashing a quick hashed torrent.
    ChunkHandle handle = m_chunk_list->get(m_position, ChunkList::get_dont_log);

    if (quick) {
      // We're not actually interested in doing any hashing, so just
      // skip what we know is not possible to hash.
      //
      // If the file does not exist then no valid error number is
      // returned.

      if (m_outstanding != 0)
        throw internal_error("HashTorrent::queue() quick hashing but m_outstanding != 0.");

      if (handle.is_valid()) {
        LT_LOG_THIS(DEBUG, "Return on handle.is_valid(): position:%u.", m_position);
        return m_chunk_list->release(&handle, ChunkList::get_dont_log);
      }
      
      if (handle.error_number().is_valid() && handle.error_number().value() != rak::error_number::e_noent) {
        LT_LOG_THIS(DEBUG, "Return on handle errno == E_NOENT: position:%u.", m_position);
        return;
      }

      m_position++;
      continue;
    }

    // If the error number is not valid, then we've just encountered a
    // file that hasn't be created/resized. Which means we ignore it
    // when doing initial hashing.
    if (handle.error_number().is_valid() && handle.error_number().value() != rak::error_number::e_noent) {
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

      LT_LOG_THIS(INFO, "Completed (error): position:%u try_quick:%u errno:%i msg:'%s'.",
                  m_position, quick, m_errno, handle.error_number().c_str());
      rak::priority_queue_erase(&taskScheduler, &m_delayChecked);
      rak::priority_queue_insert(&taskScheduler, &m_delayChecked, cachedTime);
      return;
    }

    m_position++;

    if (!handle.is_valid() && !handle.error_number().is_valid())
      throw internal_error("Hash torrent errno == 0.");

    // Missing file, skip the hash check.
    if (!handle.is_valid())
      continue;

    if (m_slot_check_chunk)
      m_slot_check_chunk(handle);

    m_outstanding++;
  }

  if (m_outstanding == 0) {
    LT_LOG_THIS(INFO, "Completed (normal): position:%u try_quick:%u.", m_position, quick);

    // Erase the scheduled item just to make sure that if hashing is
    // started again during the delay it won't cause an exception.
    rak::priority_queue_erase(&taskScheduler, &m_delayChecked);
    rak::priority_queue_insert(&taskScheduler, &m_delayChecked, cachedTime);
   }
}

}
