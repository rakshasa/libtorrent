#ifndef LIBTORRENT_CHUNK_MANAGER_H
#define LIBTORRENT_CHUNK_MANAGER_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

// TODO: Currently all chunk lists are inserted, despite the download
// not being open/active.

class LIBTORRENT_EXPORT ChunkManager : private std::vector<ChunkList*> {
public:
  using base_type = std::vector<ChunkList*>;
  using size_type = uint32_t;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::size;
  using base_type::empty;

  ChunkManager();
  ~ChunkManager();

  void                insert(ChunkList* chunkList);
  void                erase(ChunkList* chunkList);

  // The client may use these functions to affect the library's memory
  // usage by indicating how much it uses. This shouldn't really be
  // nessesary unless the client maps large amounts of memory.
  //
  // If the caller finds out the allocated memory quota isn't needed
  // due to e.g. other errors then 'deallocate_unused' must be called
  // within the context of the original 'allocate' caller in order to
  // properly be reflected when logging.
  //
  // The primary user of these functions is ChunkList.

  static constexpr int allocate_revert_log = (1 << 0);
  static constexpr int allocate_dont_log   = (1 << 1);

  bool                allocate(uint32_t size, int flags = 0);
  void                deallocate(uint32_t size, int flags = 0);

  // TODO: Add as a subscription.
  void                try_free_memory(uint64_t size);

  void                periodic_sync();

private:
  ChunkManager(const ChunkManager&) = delete;
  ChunkManager& operator=(const ChunkManager&) = delete;

  void                sync_all(int flags, uint64_t target) LIBTORRENT_NO_EXPORT;

  std::chrono::seconds m_last_try_free_memory{};
  size_type            m_last_freed_index{};
};

} // namespace torrent

#endif
