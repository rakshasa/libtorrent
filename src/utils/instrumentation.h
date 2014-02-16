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

#ifndef LIBTORRENT_UTILS_INSTRUMENTATION_H
#define LIBTORRENT_UTILS_INSTRUMENTATION_H

#include <tr1/array>

#include "torrent/common.h"
#include "torrent/utils/log.h"

namespace torrent {

enum instrumentation_enum {
  INSTRUMENTATION_MEMORY_BITFIELDS,
  INSTRUMENTATION_MEMORY_CHUNK_USAGE,
  INSTRUMENTATION_MEMORY_CHUNK_COUNT,
  INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE,
  INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT,

  INSTRUMENTATION_MINCORE_INCORE_TOUCHED,
  INSTRUMENTATION_MINCORE_INCORE_NEW,
  INSTRUMENTATION_MINCORE_NOT_INCORE_TOUCHED,
  INSTRUMENTATION_MINCORE_NOT_INCORE_NEW,
  INSTRUMENTATION_MINCORE_INCORE_BREAK,
  INSTRUMENTATION_MINCORE_SYNC_SUCCESS,
  INSTRUMENTATION_MINCORE_SYNC_FAILED,
  INSTRUMENTATION_MINCORE_SYNC_NOT_SYNCED,
  INSTRUMENTATION_MINCORE_SYNC_NOT_DEALLOCATED,
  INSTRUMENTATION_MINCORE_ALLOC_FAILED,
  INSTRUMENTATION_MINCORE_ALLOCATIONS,
  INSTRUMENTATION_MINCORE_DEALLOCATIONS,

  INSTRUMENTATION_POLLING_INTERRUPT_POKE,
  INSTRUMENTATION_POLLING_INTERRUPT_READ_EVENT,

  INSTRUMENTATION_POLLING_DO_POLL,
  INSTRUMENTATION_POLLING_DO_POLL_MAIN,
  INSTRUMENTATION_POLLING_DO_POLL_DISK,
  INSTRUMENTATION_POLLING_DO_POLL_OTHERS,

  INSTRUMENTATION_POLLING_EVENTS,
  INSTRUMENTATION_POLLING_EVENTS_MAIN,
  INSTRUMENTATION_POLLING_EVENTS_DISK,
  INSTRUMENTATION_POLLING_EVENTS_OTHERS,

  INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING,
  INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED,
  INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED,
  INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN,
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED,
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL,
  INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_ADDED,
  INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_REMOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL,

  INSTRUMENTATION_MAX_SIZE
};

extern std::tr1::array<int64_t, INSTRUMENTATION_MAX_SIZE> instrumentation_values lt_cacheline_aligned;

void instrumentation_initialize();
void instrumentation_update(instrumentation_enum type, int64_t change);
void instrumentation_tick();

//
// Implementation:
//

inline void
instrumentation_initialize() {
  instrumentation_values.assign(int64_t());
}

inline void
instrumentation_update(instrumentation_enum type, int64_t change) {
#ifdef LT_INSTRUMENTATION
  __sync_add_and_fetch(&instrumentation_values[type], change);
#endif
}

}

#endif
