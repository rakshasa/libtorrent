#ifndef LIBTORRENT_STORAGE_CONSOLIDATOR_H
#define LIBTORRENT_STORAGE_CONSOLIDATOR_H

#include "storage_file.h"
#include "storage_chunk.h"

namespace torrent {

class StorageConsolidator {
public:
  typedef std::vector<StorageFile> FileList;

  StorageConsolidator() : m_size(0), m_chunksize(1 << 16) {}
  ~StorageConsolidator();

  // We take over ownership of 'file'.
  void            add_file(File* file, uint64_t size);

  bool            resize();
  void            close();

  void            set_chunksize(uint32_t size);

  uint64_t        get_size()                     { return m_size; }
  uint32_t        get_chunk_total()               { return (m_size + m_chunksize - 1) / m_chunksize; }
  uint32_t        get_chunksize()                { return m_chunksize; }

  bool            get_chunk(StorageChunk& chunk, unsigned int b, bool wr = false, bool rd = true);

  FileList&       files()                        { return m_files; }

private:
  uint64_t        m_size;
  uint32_t        m_chunksize;
  
  FileList        m_files;
};

}

#endif

