#ifndef LIBTORRENT_DATA_HASH_TORRENT_H
#define LIBTORRENT_DATA_HASH_TORRENT_H

#include <cinttypes>
#include <functional>
#include <string>

#include "data/chunk_handle.h"
#include "torrent/utils/ranges.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class ChunkList;

class HashTorrent {
public:
  using Ranges = ranges<uint32_t>;

  using slot_chunk_handle = std::function<void(ChunkHandle)>;

  HashTorrent(ChunkList* c);
  ~HashTorrent() { clear(); }

  bool                start(bool try_quick);
  void                clear();

  bool                is_checking() const                    { return m_outstanding >= 0; }
  bool                is_checked() const;

  void                confirm_checked();

  Ranges&             hashing_ranges()                       { return m_ranges; }
  uint32_t            position() const                       { return m_position; }
  uint32_t            outstanding() const                    { return m_outstanding; }

  int                 error_number() const                   { return m_errno; }

  slot_chunk_handle&  slot_check_chunk() { return m_slot_check_chunk; }

  auto&               delay_checked()                        { return m_delay_checked; }

  void                receive_chunkdone(uint32_t index);
  void                receive_chunk_cleared(uint32_t index);

private:
  void                queue(bool quick);

  unsigned int        m_position{0};
  int                 m_outstanding{-1};
  Ranges              m_ranges;

  int                 m_errno{0};

  ChunkList*          m_chunk_list;

  slot_chunk_handle     m_slot_check_chunk;
  utils::SchedulerEntry m_delay_checked;
};

} // namespace torrent

#endif
