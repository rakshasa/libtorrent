#ifndef LIBTORRENT_DATA_CHUNK_LIST_NODE_H
#define LIBTORRENT_DATA_CHUNK_LIST_NODE_H

#include <cinttypes>

#include "torrent/utils/chrono.h"

namespace torrent {

class Chunk;

// ChunkNode can contain information like how long since it was last
// used, last synced, last checked with mincore and how many
// references there are to it.
//
// ChunkList will make sure all the nodes are cleaned up properly, so
// no dtor is needed.

class ChunkListNode {
public:
  static constexpr uint32_t invalid_index = ~uint32_t();

  bool                is_valid() const               { return m_chunk != NULL; }

  uint32_t            index() const                  { return m_index; }
  void                set_index(uint32_t idx)        { m_index = idx; }

  Chunk*              chunk() const                  { return m_chunk; }
  void                set_chunk(Chunk* c)            { m_chunk = c; }

  auto                time_modified() const                           { return m_time_modified; }
  void                set_time_modified(std::chrono::microseconds t)  { m_time_modified = t; }

  auto                time_preloaded() const                          { return m_time_preloaded; }
  void                set_time_preloaded(std::chrono::microseconds t) { m_time_preloaded = t; }

  bool                sync_triggered() const         { return m_async_triggered; }
  void                set_sync_triggered(bool v)     { m_async_triggered = v; }

  int                 references() const             { return m_references; }
  int                 dec_references()               { return --m_references; }
  int                 inc_references()               { return ++m_references; }

  int                 writable() const               { return m_writable; }
  int                 dec_writable()                 { return --m_writable; }
  int                 inc_writable()                 { return ++m_writable; }

  int                 blocking() const               { return m_blocking; }
  int                 dec_blocking()                 { return --m_blocking; }
  int                 inc_blocking()                 { return ++m_blocking; }

  void                inc_rw()                       { inc_writable(); inc_references(); }
  void                dec_rw()                       { dec_writable(); dec_references(); }

private:
  uint32_t            m_index{invalid_index};
  Chunk*              m_chunk{};

  int                 m_references{0};
  int                 m_writable{0};
  int                 m_blocking{0};

  bool                m_async_triggered{false};

  std::chrono::microseconds m_time_modified{};
  std::chrono::microseconds m_time_preloaded{};
};

} // namespace torrent

#endif
