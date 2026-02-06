#ifndef LIBTORRENT_TORRENT_SHM_CHANNEL_H
#define LIBTORRENT_TORRENT_SHM_CHANNEL_H

#include <atomic>
#include <torrent/common.h>

// Channels are intended for one writer, one reader, of data blocks less than ~1/10th the channel
// size.
//
// Querying available write space selects the largest of the contiguous free spaces, so when
// wrapping the free space may be smaller than total free space.

namespace torrent::shm {

class LIBTORRENT_EXPORT Channel {
public:
  struct [[gnu::packed]] header_type {
    uint32_t    size{};
    uint32_t    id{};
    char        data[];
  };

  static constexpr size_t header_size     = sizeof(header_type);
  static constexpr size_t cache_line_size = std::hardware_destructive_interference_size;

  void                initialize(void* addr, size_t size);

  // There will always be at least one (unusable) cache line free, and headers are not included.
  //
  // Only use this for a rough estimate of available space.
  uint32_t            available_write();

  bool                write(uint32_t id, uint32_t size, void* data);

  header_type*        read_header();
  void                consume_header(header_type* header);

protected:
  // These are offset by size of ChannelBase.
  void*                 m_addr{};
  uint32_t              m_size{};

  std::atomic<uint32_t> m_read_offset{};
  std::atomic<uint32_t> m_write_offset{};
};

} // namespace torrent::shm

#endif // LIBTORRENT_TORRENT_SHM_CHANNEL_H
