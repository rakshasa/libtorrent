#ifndef LIBTORRENT_TORRENT_SYSTEM_IPC_CHANNEL_H
#define LIBTORRENT_TORRENT_SYSTEM_IPC_CHANNEL_H

#include <torrent/common.h>

// Channels are intended for one writer, one reader, of data blocks less than ~1/10th the channel
// size.
//
// Querying available write space selects the largest of the contiguous free spaces, so when
// wrapping the free space may be smaller than total free space.

namespace torrent::system::ipc {

class LIBTORRENT_EXPORT Channel {
public:
  struct [[gnu::packed]] header_type {
    uint32_t    size{};
    uint32_t    id{};
    char        data[];
  };

  static constexpr size_t   header_size     = sizeof(header_type);
  static constexpr size_t   cache_line_size = LT_SMP_CACHE_BYTES;

  static constexpr uint32_t flag_polling = 0x1;

  void                initialize(void* addr, size_t size);

  auto&               consumer_state();

  // There will always be at least one (unusable) cache line free, and headers are not included.
  //
  // Only use this for a rough estimate of available space.
  uint32_t            available_write();

  bool                can_write(uint32_t size);

  bool                write(uint32_t id, uint32_t size, void* data);

  header_type*        read_header();
  void                consume_header(header_type* header);

private:
  Channel() = delete;
  ~Channel() = delete;

  inline header_type*   header_at_offset(uint32_t offset);

  // Constant values, offset by sizeof(Channel).

  void*                 m_addr{};
  uint32_t              m_size{};
  uint32_t              m_write_threshold{};

  // Mutable state:

  std::atomic<uint32_t> m_read_offset{};
  std::atomic<uint32_t> m_write_offset{};

  std::atomic<uint32_t> m_consumer_state{};
};

inline auto& Channel::consumer_state() { return m_consumer_state; }

} // namespace torrent::system::ipc

#endif
