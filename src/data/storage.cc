#include "config.h"

#include "torrent/exceptions.h"

#include "storage.h"

namespace torrent {

Storage::Storage() :
  m_consolidator(new StorageConsolidator()) {
}

Storage::~Storage() {
  close();

  delete m_consolidator;
}

void
Storage::set_chunksize(uint32_t s) {
  m_consolidator->set_chunksize(s);

  m_anchors.resize(get_chunk_total());
}

Storage::Chunk
Storage::get_chunk(unsigned int b, bool wr, bool rd) {
  if (b >= m_anchors.size())
    throw internal_error("Chunk out of range in Storage");

  if (m_anchors[b].is_valid())
    return Chunk(m_anchors[b]);

  Chunk chunk(new StorageChunk(b));

  if (!m_consolidator->get_chunk(*chunk, b, wr, rd))
    return Chunk();
  
  chunk.anchor(m_anchors[b]);

  return chunk;
}

Storage::FileList& 
Storage::get_files() {
  return m_consolidator->get_files();
}

}
