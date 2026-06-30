#ifndef LIBTORRENT_CHUNK_UTILS_H
#define LIBTORRENT_CHUNK_UTILS_H

#include <vector>

#include <torrent/common.h>
#include <torrent/download.h>

namespace RTORRENT_EXPORT torrent {

class ChunkList;

struct vm_mapping {
  void* ptr;
  uint64_t length;
};

// Change to ChunkList* when that becomes part of the public API.

std::vector<vm_mapping> chunk_list_mapping(Download* download);

struct chunk_info_result {
  Download download;

  uint32_t chunk_index;
  uint32_t chunk_offset;

  const char* file_path;
  uint64_t file_offset;
};

chunk_info_result chunk_list_address_info(void* address);

} // namespace torrent

#endif
