#ifndef LIBTORRENT_HASH_CHUNK_H
#define LIBTORRENT_HASH_CHUNK_H

#include <memory>

#include "torrent/exceptions.h"

#include "chunk.h"
#include "chunk_handle.h"

namespace torrent {

// This class interface assumes we're always going to check the whole
// chunk. All we need is control of the (non-)blocking nature, and other
// stuff related to performance and responsiveness.

class ChunkListNode;
class Sha1;

class HashChunk {
public:
  ~HashChunk();
  HashChunk(ChunkHandle h);

  void                set_chunk(ChunkHandle h);

  ChunkHandle*        chunk()                                 { return &m_chunk; }
  ChunkHandle&        handle()                                { return m_chunk; }
  void                hash_c(char* buffer);

  // If force is true, then the return value is always true.
  bool                perform(uint32_t length, bool force = true);

  void                advise_willneed(uint32_t length);

  uint32_t            remaining();

private:
  inline uint32_t     remaining_part(Chunk::iterator itr, uint32_t pos);
  uint32_t            perform_part(Chunk::iterator itr, uint32_t length);

  uint32_t            m_position;

  ChunkHandle         m_chunk;
  std::unique_ptr<Sha1> m_hash;
};

inline uint32_t
HashChunk::remaining_part(Chunk::iterator itr, uint32_t pos) {
  return itr->size() - (pos - itr->position());
}

inline uint32_t
HashChunk::remaining() {
  if (!m_chunk.is_loaded())
    throw internal_error("HashChunk::remaining(...) called on an invalid chunk");

  return m_chunk.chunk()->chunk_size() - m_position;
}

}

#endif

