#include "config.h"

#include "torrent/exceptions.h"

#include "storage.h"
#include "storage_consolidator.h"

namespace torrent {

Storage::Storage() :
  m_consolidator(new StorageConsolidator()) {
}

Storage::~Storage() {
  close();

  delete m_consolidator;
}

void
Storage::add_file(File* file, uint64_t length) {
  m_consolidator->add_file(file, length);
}

bool
Storage::resize() {
  return m_consolidator->resize();
}

void
Storage::close() {
  m_anchors.clear();
  m_consolidator->close();
}

void
Storage::set_chunksize(uint32_t s) {
  m_consolidator->set_chunksize(s);

  m_anchors.resize(get_chunkcount());
}

uint64_t
Storage::get_size() {
  return m_consolidator->get_size();
}

uint32_t
Storage::get_chunkcount() {
  return m_consolidator->get_chunkcount();
}

uint32_t
Storage::get_chunksize() {
  return m_consolidator->get_chunksize();
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
Storage::files() {
  return m_consolidator->files();
}

}
