#include "config.h"

#include <algo/common.h>

#include "exceptions.h"
#include "storage_chunk.h"

using namespace algo;

namespace torrent {

StorageChunk::~StorageChunk() {
  std::for_each(m_nodes.begin(), m_nodes.end(),
		delete_on());
}

StorageChunk::Node& StorageChunk::get_position(unsigned int pos) {
  if (pos >= m_size)
    throw internal_error("Tried to get StorageChunk position out of range.");

  Nodes::iterator itr = m_nodes.begin();

  while (itr != m_nodes.end()) {
    if (pos < (*itr)->position + (*itr)->fileChunk.length())
      return **itr;
  }
  
  throw internal_error("StorageChunk might be mangled, get_position failed horribly");
}

// Each add calls vector's reserve adding 1. This should keep
// the size of the vector at exactly what we need. Though it
// will require a few more cycles, it won't matter as we only
// rarely have more than 1 or 2 nodes.
FileChunk& StorageChunk::add_file(unsigned int length) {
  m_nodes.reserve(m_nodes.size() + 1);

  m_size += length;

  return (*m_nodes.insert(m_nodes.end(), new Node(m_size - length)))->fileChunk;
}

}
