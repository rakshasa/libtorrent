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

#include "instrumentation.h"

namespace torrent {

std::tr1::array<int64_t, INSTRUMENTATION_MAX_SIZE> instrumentation_values lt_cacheline_aligned;

inline int64_t
instrumentation_fetch_and_clear(instrumentation_enum type) {
#ifdef LT_INSTRUMENTATION
  return __sync_fetch_and_and(&instrumentation_values[type], int64_t());
#endif
}

void
instrumentation_tick() {
  // Since the values are updated with __sync_add, they can be read
  // without any memory barriers.
  lt_log_print(LOG_INSTRUMENTATION_MEMORY,
               "%" PRIi64 " %" PRIi64 " %" PRIi64  " %" PRIi64 " %" PRIi64,
               instrumentation_values[INSTRUMENTATION_MEMORY_CHUNK_USAGE],
               instrumentation_values[INSTRUMENTATION_MEMORY_CHUNK_COUNT],
               instrumentation_values[INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE],
               instrumentation_values[INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT],
               instrumentation_values[INSTRUMENTATION_MEMORY_BITFIELDS]);

  lt_log_print(LOG_INSTRUMENTATION_MINCORE,
               "%"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %" PRIi64 " %" PRIi64,
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_TOUCHED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_NEW),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_NOT_INCORE_TOUCHED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_NOT_INCORE_NEW),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_BREAK),

               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_SUCCESS),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_FAILED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_NOT_SYNCED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_NOT_DEALLOCATED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_ALLOC_FAILED),

               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_ALLOCATIONS),
               instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_DEALLOCATIONS));

  lt_log_print(LOG_INSTRUMENTATION_POLLING,
               "%"  PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64,
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_INTERRUPT_POKE),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_INTERRUPT_READ_EVENT),

               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_MAIN),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_DISK),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_OTHERS),

               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_MAIN),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_DISK),
               instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_OTHERS));

  lt_log_print(LOG_INSTRUMENTATION_TRANSFERS,
               "%"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64,

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN),

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL],

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL]);
}

}
