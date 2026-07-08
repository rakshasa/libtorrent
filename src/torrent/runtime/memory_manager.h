#ifndef LIBTORRENT_TORRENT_RUNTIME_MEMORY_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_MEMORY_MANAGER_H

#include <torrent/common.h>

namespace torrent::runtime {

MemoryManager* memory_manager() LIBTORRENT_EXPORT;

class LIBTORRENT_EXPORT MemoryManager {
public:

  // Should we allow the client to reserve some memory?

  uint64_t            memory_usage() const;
  uint32_t            memory_block_count() const;

  uint64_t            max_memory_usage() const;
  void                set_max_memory_usage(uint64_t bytes);

  void                account_memory_block(uint64_t bytes);
  void                release_memory_block(uint64_t bytes);

protected:
  friend class torrent::RuntimeManager;

  MemoryManager();
  ~MemoryManager();

private:
  std::atomic<uint64_t> m_memory_usage{};
  std::atomic<uint32_t> m_memory_block_count{};
  std::atomic<uint64_t> m_max_memory_usage{};
};

inline uint64_t MemoryManager::memory_usage() const       { return m_memory_usage.load(std::memory_order_acquire); }
inline uint32_t MemoryManager::memory_block_count() const { return m_memory_block_count.load(std::memory_order_acquire); }
inline uint64_t MemoryManager::max_memory_usage() const   { return m_max_memory_usage.load(std::memory_order_acquire); }

} // namespace torrent::runtime

#endif

