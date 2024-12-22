#include "config.h"

#include "instrumentation.h"

namespace torrent {

std::array<std::atomic_int64_t, INSTRUMENTATION_MAX_SIZE> instrumentation_values;

inline int64_t
instrumentation_fetch_and_clear(instrumentation_enum type) {
#ifdef LT_INSTRUMENTATION
  return instrumentation_values[type].exchange(0);
#else
  return 0;
#endif
}

void
instrumentation_tick() {
  lt_log_print(LOG_INSTRUMENTATION_MEMORY,
               "%" PRIi64 " %" PRIi64 " %" PRIi64  " %" PRIi64 " %" PRIi64,
               instrumentation_values[INSTRUMENTATION_MEMORY_CHUNK_USAGE].load(),
               instrumentation_values[INSTRUMENTATION_MEMORY_CHUNK_COUNT].load(),
               instrumentation_values[INSTRUMENTATION_MEMORY_HASHING_CHUNK_USAGE].load(),
               instrumentation_values[INSTRUMENTATION_MEMORY_HASHING_CHUNK_COUNT].load(),
               instrumentation_values[INSTRUMENTATION_MEMORY_BITFIELDS].load());

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
               "%"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %"  PRIi64 " %" PRIi64 " %" PRIi64 " %" PRIi64
               " %" PRIi64,

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_DELEGATED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED),

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_MOVED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL].load(),

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_MOVED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_TOTAL].load(),

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_MOVED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_TOTAL].load(),

               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_ADDED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_MOVED),
               instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_REMOVED),
               instrumentation_values[INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_TOTAL].load(),

               instrumentation_values[INSTRUMENTATION_TRANSFER_PEER_INFO_UNACCOUNTED].load());
}

void
instrumentation_reset() {
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_TOUCHED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_NEW);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_NOT_INCORE_TOUCHED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_NOT_INCORE_NEW);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_INCORE_BREAK);

  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_SUCCESS);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_FAILED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_NOT_SYNCED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_SYNC_NOT_DEALLOCATED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_ALLOC_FAILED);

  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_ALLOCATIONS);
  instrumentation_fetch_and_clear(INSTRUMENTATION_MINCORE_DEALLOCATIONS);

  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_INTERRUPT_POKE);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_INTERRUPT_READ_EVENT);

  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_MAIN);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_DISK);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_DO_POLL_OTHERS);

  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_MAIN);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_DISK);
  instrumentation_fetch_and_clear(INSTRUMENTATION_POLLING_EVENTS_OTHERS);

  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_DELEGATED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED);

  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_MOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_ADDED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_MOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_REMOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_ADDED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_MOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_REMOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_ADDED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_MOVED);
  instrumentation_fetch_and_clear(INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_REMOVED);
}

}
