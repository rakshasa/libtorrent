#include "config.h"

#include "torrent/runtime/memory_manager.h"

#include <cassert>
#include <sys/resource.h>

#include "torrent/exceptions.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                        \
  lt_log_print(LOG_SYSTEM_POLL, "memory-manager: " log_fmt, __VA_ARGS__);

namespace {

uint64_t
get_adjusted_rlimit_as() {
#ifdef RLIMIT_AS
  rlimit rlp;

  if (getrlimit(RLIMIT_AS, &rlp) == -1 || rlp.rlim_cur == RLIM_INFINITY)
    return UINT64_MAX;

  if (rlp.rlim_cur > UINT64_MAX)
    return UINT64_MAX - UINT64_MAX / 8;

  return rlp.rlim_cur - rlp.rlim_cur / 8;

#else
  return UINT64_MAX;
#endif
}

}

namespace torrent::runtime {

MemoryManager::MemoryManager() {
  m_max_memory_usage = std::min(get_adjusted_rlimit_as(), uint64_t{DEFAULT_ADDRESS_SPACE_SIZE} << 20);
}

MemoryManager::~MemoryManager() {
  assert(m_memory_usage == 0);
  assert(m_memory_block_count == 0);
}

void
MemoryManager::set_max_memory_usage(uint64_t bytes) {
  uint64_t rlimit_as = get_adjusted_rlimit_as();

  if (bytes > rlimit_as)
    throw input_error("set_max_memory_usage: memory limit exceeds (RLIMIT_AS - RLIMIT_AS/8) : " + std::to_string(bytes) + " > " + std::to_string(rlimit_as));

  if (bytes < (uint64_t{512} << 20))
    throw input_error("set_max_memory_usage: memory limit too low, must be at least 512 MB : " + std::to_string(bytes));

  m_max_memory_usage = bytes;

  LT_LOG("set_max_memory_usage: new limit: %" PRIu64, m_max_memory_usage.load());
}

void
MemoryManager::account_memory_block(uint64_t bytes) {
  m_memory_block_count++;
  m_memory_usage += bytes;
}

void
MemoryManager::release_memory_block(uint64_t bytes) {
  auto previous_usage = m_memory_usage.fetch_sub(bytes,   std::memory_order_acq_rel);
  auto previous_count = m_memory_block_count.fetch_sub(1, std::memory_order_acq_rel);

  if (previous_count == 0)
    throw internal_error("MemoryManager::release_memory_block() previous_count == 0.");

  if (previous_usage < bytes)
    throw internal_error("MemoryManager::release_memory_block() previous_usage < bytes.");
}

uint64_t
MemoryManager::sync_safe_free_diskspace() const {
  return m_memory_usage + (uint64_t{512} << 20);
}

void
MemoryManager::set_timeout_sync(uint32_t seconds) {
  if (seconds > 3600)
    throw input_error("set_timeout_sync: invalid timeout, must be between 0 and 3600 seconds : " + std::to_string(seconds));

  m_timeout_sync = seconds;
}

void
MemoryManager::set_preload_type(uint32_t type) {
  if (type > 2)
    throw input_error("set_preload_type: invalid type : " + std::to_string(type));

  m_preload_type = type;
}

void
MemoryManager::set_preload_min_size(uint32_t bytes) {
  if (bytes < (1 << 10))
    throw input_error("set_preload_min_size: invalid size, must be at least 1 KB : " + std::to_string(bytes));

  m_preload_min_size = bytes;
}

void
MemoryManager::set_preload_required_rate(uint32_t bytes) {
  if (bytes < (1 << 10))
    throw input_error("set_preload_required_rate: invalid rate, must be at least 1 KB/s : " + std::to_string(bytes));

  m_preload_required_rate = bytes;
}

void
MemoryManager::account_sync_queue(uint64_t bytes) {
  m_sync_queue_block_count++;
  m_sync_queue_memory_usage += bytes;
}

void
MemoryManager::release_sync_queue(uint32_t count, uint32_t chunk_size) {
  uint64_t bytes = static_cast<uint64_t>(count) * chunk_size;

  auto previous_usage = m_sync_queue_memory_usage.fetch_sub(bytes, std::memory_order_acq_rel);
  auto previous_count = m_sync_queue_block_count.fetch_sub(count, std::memory_order_acq_rel);

  if (previous_count < count)
    throw internal_error("MemoryManager::release_sync_queue() previous_count < count.");

  if (previous_usage < bytes)
    throw internal_error("MemoryManager::release_sync_queue() previous_usage < bytes.");
}

} // namespace torrent::runtime
