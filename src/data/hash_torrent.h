#ifndef LIBTORRENT_DATA_HASH_TORRENT_H
#define LIBTORRENT_DATA_HASH_TORRENT_H

#include <cinttypes>
#include <functional>
#include <string>
#include <rak/priority_queue_default.h>

#include "data/chunk_handle.h"
#include "torrent/utils/ranges.h"

namespace torrent {

class ChunkList;

class HashTorrent {
public:
  typedef ranges<uint32_t> Ranges;

  typedef std::function<void (ChunkHandle)> slot_chunk_handle;

  HashTorrent(ChunkList* c);
  ~HashTorrent() { clear(); }

  bool                start(bool try_quick);
  void                clear();

  bool                is_checking()                          { return m_outstanding >= 0; }
  bool                is_checked();

  void                confirm_checked();

  Ranges&             hashing_ranges()                       { return m_ranges; }
  uint32_t            position() const                       { return m_position; }
  uint32_t            outstanding() const                    { return m_outstanding; }

  int                 error_number() const                   { return m_errno; }

  slot_chunk_handle&  slot_check_chunk() { return m_slot_check_chunk; }

  rak::priority_item& delay_checked()                        { return m_delayChecked; }

  void                receive_chunkdone(uint32_t index);
  void                receive_chunk_cleared(uint32_t index);

private:
  void                queue(bool quick);

  unsigned int        m_position;
  int                 m_outstanding;
  Ranges              m_ranges;

  int                 m_errno;

  ChunkList*          m_chunk_list;

  slot_chunk_handle   m_slot_check_chunk;

  rak::priority_item  m_delayChecked;
};

}

#endif
