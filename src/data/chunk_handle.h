#ifndef LIBTORRENT_DATA_CHUNK_HANDLE_H
#define LIBTORRENT_DATA_CHUNK_HANDLE_H

#include "chunk_list_node.h"

namespace torrent {

class ChunkListNode;

class ChunkHandle {
public:
  ChunkHandle(ChunkListNode* c = NULL, bool wr = false, bool blk = false) :
    m_node(c), m_writable(wr), m_blocking(blk) {}

  bool                is_valid() const        { return m_node != NULL; }
  bool                is_loaded() const       { return m_node != NULL && m_node->is_valid(); }
  bool                is_writable() const     { return m_writable; }
  bool                is_blocking() const     { return m_blocking; }

  void                clear()                 { m_node = NULL; m_writable = false; m_blocking = false; }

  int                 error_number() const    { return m_errno; }
  void                set_error_number(int e) { m_errno = e; }

  ChunkListNode*      object() const          { return m_node; }
  Chunk*              chunk() const           { return m_node->chunk(); }

  uint32_t            index() const           { return m_node->index(); }

  static ChunkHandle  from_error(int e)       { ChunkHandle h; h.set_error_number(e); return h; }

private:
  ChunkListNode*      m_node;
  bool                m_writable;
  bool                m_blocking;

  int                 m_errno;
};

} // namespace torrent

#endif
