#ifndef LIBTORRENT_STORAGE_H
#define LIBTORRENT_STORAGE_H

#include <vector>
#include <inttypes.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"
#include "storage_consolidator.h"
#include "storage_file.h"

namespace torrent {

class File;
class StorageConsolidator;

// TODO: Make Consolidator a private base class.

class Storage {
public:
  typedef std::vector<StorageFile>        FileList;
  typedef algo::RefAnchored<StorageChunk> Chunk;
  typedef algo::RefAnchor<StorageChunk>   Anchor;

  Storage();
  ~Storage();

  // We take over ownership of 'file'.
  void      add_file(File* file, uint64_t length)   { m_consolidator->add_file(file, length); }

  bool      resize()                                { return m_consolidator->resize(); }
  bool      sync()                                  { return m_consolidator->sync(); }
  void      close()                                 { m_anchors.clear(); m_consolidator->close(); }

  // Call this when all files have been added.
  void      set_chunksize(uint32_t s);
  
  uint64_t  get_size()                              { return m_consolidator->get_size(); }
  uint32_t  get_chunk_total()                       { return m_consolidator->get_chunk_total(); }
  uint32_t  get_chunk_size()                        { return m_consolidator->get_chunk_size(); }

  Chunk     get_chunk(unsigned int b, bool wr = false, bool rd = true);

  FileList& get_files();

private:
  StorageConsolidator* m_consolidator;
  std::vector<Anchor>  m_anchors;
};

}

#endif
  
