#ifndef LIBTORRENT_STORAGE_CONSOLIDATOR_H
#define LIBTORRENT_STORAGE_CONSOLIDATOR_H

#include "file.h"
#include "storage_chunk.h"

namespace torrent {

class StorageConsolidator {
public:
  struct Node {
    Node(File* f, uint64_t p, uint64_t l) : file(f), position(p), length(l) {}

    File* file;
    uint64_t position;
    uint64_t length;
  };

  typedef std::vector<Node> Files;

  StorageConsolidator() : m_size(0), m_chunksize(1 << 16) {}
  ~StorageConsolidator();

  // We take over ownership of 'file'.
  void         add_file(File* file, uint64_t length);

  bool         resize();
  void         close();

  void         set_chunksize(unsigned int size);

  uint64_t     get_size()                     { return m_size; }
  unsigned int get_chunkcount()               { return (m_size + m_chunksize - 1) / m_chunksize; }
  unsigned int get_chunksize()                { return m_chunksize; }

  bool         get_chunk(StorageChunk& chunk, unsigned int b, bool wr = false, bool rd = true);

  const Files& files()                        { return m_files; }

private:
  uint64_t     m_size;
  unsigned int m_chunksize;
  
  Files        m_files;
};

}

#endif

