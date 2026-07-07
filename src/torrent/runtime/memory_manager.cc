#include "config.h"

#include "torrent/runtime/memory_manager.h"

#include <cassert>
#include <sys/resource.h>

#include "torrent/exceptions.h"

namespace {

uint64_t
get_adjusted_rlimit_as() {
#ifdef RLIMIT_AS
  rlimit rlp;

  if (getrlimit(RLIMIT_AS, &rlp) == -1 || rlp.rlim_cur == RLIM_INFINITY)
    return UINT64_MAX;

  if (rlp.rlim_cur > UINT64_MAX)
    return UINT64_MAX - UINT64_MAX / 6;

  return rlp.rlim_cur - rlp.rlim_cur / 6;

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
    throw input_error("set_max_memory_usage: memory limit exceeds (RLIMIT_AS - RLIMIT_AS/6) : " + std::to_string(bytes) + " > " + std::to_string(rlimit_as));

  if (bytes < (uint64_t{128} << 20))
    throw input_error("set_max_memory_usage: memory limit too low, must be at least 128mb : " + std::to_string(bytes));

  m_max_memory_usage = bytes;
}

} // namespace torrent::runtime
