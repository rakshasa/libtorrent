#ifndef LIBTORRENT_TORRENT_RUNTIME_MEMORY_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_MEMORY_MANAGER_H

#include <torrent/common.h>

namespace torrent::runtime {

MemoryManager* memory_manager() LIBTORRENT_EXPORT;

class LIBTORRENT_EXPORT MemoryManager {
public:

  // Should we allow the client to reserve some memory?

  //
  // Memory Management:
  //

  uint64_t            memory_usage() const;
  uint32_t            memory_block_count() const;

  uint64_t            max_memory_usage() const;
  void                set_max_memory_usage(uint64_t bytes);

  void                account_memory_block(uint64_t bytes);
  void                release_memory_block(uint64_t bytes);

  //
  // Disk Sync and Preload Management:
  //

  uint32_t            sync_queue_block_count() const;
  uint64_t            sync_queue_memory_usage() const;

  uint64_t            sync_safe_free_diskspace() const;

  bool                safe_sync() const;
  void                set_safe_sync(bool state);

  // Set the wait time after last write to a chunk before attempting to sync.
  //
  // Longer timeout allows the kernel to sync at its convenience.
  auto                timeout_sync() const;
  void                set_timeout_sync(uint32_t seconds);

  // Set to 0 to disable preloading.
  //
  // How the value is used is yet to be determined, but it won't be able to use actual requests in
  // the request queue as we can easily stay ahead of it causing preloading to fail.
  uint32_t            preload_type() const;
  void                set_preload_type(uint32_t preload_type);

  uint32_t            preload_min_size() const;
  void                set_preload_min_size(uint32_t bytes);

  // Required rate (per integer MB of remaining chunk) before attempting to preload chunk.
  uint32_t            preload_required_rate() const;
  void                set_preload_required_rate(uint32_t bytes);

protected:
  friend class torrent::ChunkList;
  friend class torrent::PeerConnectionBase;
  friend class torrent::RuntimeManager;

  MemoryManager();
  ~MemoryManager();

  void                account_sync_queue(uint64_t bytes);
  void                release_sync_queue(uint32_t count, uint32_t chunk_size);

  void                increment_stats_preloaded();
  void                increment_stats_not_preloaded();

private:
  std::atomic<uint64_t> m_memory_usage{};
  std::atomic<uint32_t> m_memory_block_count{};
  std::atomic<uint64_t> m_max_memory_usage{};

  std::atomic<uint32_t> m_sync_queue_block_count{};
  std::atomic<uint64_t> m_sync_queue_memory_usage{};

  std::atomic<bool>     m_safe_sync{false};
  std::atomic<uint32_t> m_timeout_sync{600};

  std::atomic<uint32_t> m_preload_type{0};
  std::atomic<uint32_t> m_preload_min_size{256 << 10};
  std::atomic<uint32_t> m_preload_required_rate{5 << 10};

  std::atomic<uint32_t> m_stats_preloaded{};
  std::atomic<uint32_t> m_stats_not_preloaded{};
};

inline uint64_t MemoryManager::memory_usage() const            { return m_memory_usage.load(std::memory_order_acquire); }
inline uint32_t MemoryManager::memory_block_count() const      { return m_memory_block_count.load(std::memory_order_acquire); }
inline uint64_t MemoryManager::max_memory_usage() const        { return m_max_memory_usage.load(std::memory_order_acquire); }
inline uint32_t MemoryManager::sync_queue_block_count() const  { return m_sync_queue_block_count.load(std::memory_order_acquire); }
inline uint64_t MemoryManager::sync_queue_memory_usage() const { return m_sync_queue_memory_usage.load(std::memory_order_acquire); }

inline bool     MemoryManager::safe_sync() const               { return m_safe_sync.load(std::memory_order_acquire); }
inline void     MemoryManager::set_safe_sync(bool state)       { m_safe_sync.store(state, std::memory_order_release); }
inline auto     MemoryManager::timeout_sync() const            { return std::chrono::seconds(m_timeout_sync.load(std::memory_order_acquire)); }
inline uint32_t MemoryManager::preload_type() const            { return m_preload_type.load(std::memory_order_acquire); }
inline uint32_t MemoryManager::preload_min_size() const        { return m_preload_min_size.load(std::memory_order_acquire); }
inline uint32_t MemoryManager::preload_required_rate() const   { return m_preload_required_rate.load(std::memory_order_acquire); }

inline void     MemoryManager::increment_stats_preloaded()     { m_stats_preloaded.fetch_add(1, std::memory_order_acq_rel); }
inline void     MemoryManager::increment_stats_not_preloaded() { m_stats_not_preloaded.fetch_add(1, std::memory_order_acq_rel); }

} // namespace torrent::runtime

#endif

