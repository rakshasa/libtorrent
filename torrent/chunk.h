#ifndef LIBTORRENT_CHUNK_H
#define LIBTORRENT_CHUNK_H

#include <list>
#include <string>

#include "storage_block.h"

namespace torrent {

// TODO: Reference count Chunk?

class Chunk {
public:
  typedef std::pair<unsigned int,StorageBlock> Part;
  
  Chunk() : m_index(-1) {}
  Chunk(int index) : m_index(index) {}

  void add(const StorageBlock& sb);
  Part& get(unsigned int offset);

  int index() const { return m_index; }
  unsigned int length() const;
  std::string hash();

  std::list<Part>& parts() { return m_parts; }

private:
  int m_index;

  std::list<Part> m_parts;
};

}

#endif // LIBTORRENT_CHUNK_H
