#ifndef LIBTORRENT_HASH_CHUNK_H
#define LIBTORRENT_HASH_CHUNK_H

#include <algo/ref_anchored.h>

#include "hash_compute.h"

namespace torrent {

// This class interface assumes we're always going to check the whole
// chunk. All we need is control of the (non-)blocking nature, and other
// stuff related to performance and responsiveness.

class StorageChunk;

class HashChunk {
public:
  typedef algo::RefAnchored<StorageChunk> Chunk;

  HashChunk() : m_chunk(NULL)     {}
  
  void         set_chunk(Chunk c) { m_position = 0; m_chunk = c; m_hash.init(); }

  Chunk        get_chunk()        { return m_chunk; }

  // If force is true, then the return value is always true.
  bool         process(unsigned int length, bool force = false);

  bool         willneed(unsigned int length);

  unsigned int remaining_chunk();
  unsigned int remaining_file();

private:
  unsigned int m_position;

  Chunk        m_chunk;
  HashCompute  m_hash;
};

}

#endif

