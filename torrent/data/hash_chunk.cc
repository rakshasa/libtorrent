#include "config.h"

#include "exceptions.h"
#include "hash_chunk.h"
#include "storage_chunk.h"

namespace torrent {

bool HashChunk::process(unsigned int length, bool force) {
  if (!m_chunk.is_valid() ||
      !m_chunk->is_valid())
    throw internal_error("HashChunk::process_force(...) called on an invalid chunk");

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::process_force(...) received length out of range");
  
  while (length) {
    StorageChunk::Node& node = m_chunk->get_position(m_position);

    unsigned int l = std::min(length, node.length - (m_position - node.position));

    // Usually we only have one or two files in a chunk, so we don't mind the if statement
    // inside the loop.

    if (force) {
      m_hash.update(node.chunk.begin() + m_position - node.position, l);

      length     -= l;
      m_position += l;

    } else {
      unsigned int incore, size = node.chunk.page_touched(m_position - node.position, l);
      unsigned char buf[size];

      // TODO: We're borking here with NULL node.
      node.chunk.incore(buf, m_position - node.position, l);

      for (incore = 0; incore < size; ++incore)
	if (!buf[incore])
	  break;

      if (incore == 0)
	return false;

      l = std::min(l, incore * node.chunk.page_size() - node.chunk.page_align(m_position - node.position));

      m_hash.update(node.chunk.begin() + m_position - node.position, l);

      length     -= l;
      m_position += l;
      
      if (incore < size)
	return false;
    }
  }

  return true;
}


// Warning: Can paralyze linux 2.4.20.
bool HashChunk::willneed(unsigned int length) {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::willneed(...) called on an invalid chunk");

  if (m_position + length > m_chunk->get_size())
    throw internal_error("HashChunk::willneed(...) received length out of range");

  unsigned int pos = m_position;

  while (length) {
    StorageChunk::Node& node = m_chunk->get_position(pos);

    unsigned int l = std::min(length, length - (pos - node.position));

    node.chunk.advise(pos, l, FileChunk::advice_willneed);

    pos    += l;
    length -= l;
  }

  return true;
}

unsigned int HashChunk::remaining_chunk() {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::remaining_chunk() called on an invalid chunk");

  return m_chunk->get_size() - m_position;
}

unsigned int HashChunk::remaining_file() {
  if (!m_chunk.is_valid())
    throw internal_error("HashChunk::remaining_chunk() called on an invalid chunk");
  
  if (m_position == m_chunk->get_size())
    return 0;

  StorageChunk::Node& node = m_chunk->get_position(m_position);

  return node.length - (m_position - node.position);
}    

}
