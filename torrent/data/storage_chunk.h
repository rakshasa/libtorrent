#ifndef LIBTORRENT_STORAGE_CHUNK_H
#define LIBTORRENT_STORAGE_CHUNK_H

#include <vector>
#include "file_chunk.h"

namespace torrent {

class StorageChunk {
public:
  friend class StorageConsolidator;

  struct Node {
    Node(unsigned int pos) : position(pos) {}

    FileChunk    chunk;
    unsigned int position;
    unsigned int length;
  };

  typedef std::vector<Node*> Nodes;

  StorageChunk(unsigned int index) : m_index(index), m_size(0) {}
  ~StorageChunk()                              { clear(); }

  bool is_valid()                              { return m_size; }

  int          get_index()                     { return m_index; }
  unsigned int get_size()                      { return m_size; }
  Nodes&       get_nodes()                     { return m_nodes; }

  // Get the Node that contains 'pos'.
  Node&        get_position(unsigned int pos);

  void clear();

protected:
  // When adding file chunks, make sure you clear this object if *any*
  // fails.
  FileChunk&   add_file(unsigned int length);

private:
  StorageChunk(const StorageChunk&);
  void operator = (const StorageChunk&);
  
  unsigned int m_index;
  unsigned int m_size;
  Nodes        m_nodes;
};

}

#endif

  
