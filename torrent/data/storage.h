#ifndef LIBTORRENT_STORAGE_H
#define LIBTORRENT_STORAGE_H

#include <vector>
#include <inttypes.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"
#include "storage_file.h"

namespace torrent {

class File;
class StorageConsolidator;

class Storage {
public:
  typedef std::vector<StorageFile>        FileList;
  typedef algo::RefAnchored<StorageChunk> Chunk;

  Storage();
  ~Storage();

  // We take over ownership of 'file'.
  void         add_file(File* file, uint64_t length);

  bool         resize();
  void         close();

  // Call this when all files have been added.
  void         set_chunksize(unsigned int s);
  
  uint64_t     get_size();
  unsigned int get_chunkcount();
  unsigned int get_chunksize();

  Chunk get_chunk(unsigned int b, bool wr = false, bool rd = true);

  FileList&    files();

private:
  StorageConsolidator*                        m_consolidator;

  std::vector<algo::RefAnchor<StorageChunk> > m_anchors;
};

}

#endif
  
