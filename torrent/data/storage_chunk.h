#ifndef LIBTORRENT_STORAGE_CHUNK_H
#define LIBTORRENT_STORAGE_CHUNK_H

#include <vector>
#include "file_chunk.h"

namespace torrent {

class StorageChunk {
public:
  struct Node {
    Node(unsigned int pos) : position(pos) {}

    FileChunk    fileChunk;
    unsigned int position;
  };

  typedef std::vector<Node*> Nodes;

  StorageChunk() : m_size(0) {}
  ~StorageChunk();

  unsigned int get_size()                      { return m_size; }
  Nodes&       get_nodes()                     { return m_nodes; }

  // Get the Node that contains 'pos'.
  Node&        get_position(unsigned int pos);

protected:
  FileChunk&   add_file(unsigned int length);

private:
  unsigned int m_size;

  Nodes        m_nodes;
};

}

#endif

  
