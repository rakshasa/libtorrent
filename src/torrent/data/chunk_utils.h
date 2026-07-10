#ifndef LIBTORRENT_CHUNK_UTILS_H
#define LIBTORRENT_CHUNK_UTILS_H

#include <vector>

#include <torrent/common.h>
#include <torrent/download.h>

namespace torrent {

class ChunkList;

struct vm_mapping {
  void* ptr;
  uint64_t length;
};

// Change to ChunkList* when that becomes part of the public API.

std::vector<vm_mapping> chunk_list_mapping(Download* download) LIBTORRENT_EXPORT;

struct chunk_info_result {
  Download download;

  uint32_t chunk_index;
  uint32_t chunk_offset;

  const char* file_path;
  uint64_t file_offset;

  // void* chunk_begin;
  // void* chunk_end;

  // int prot;
};

chunk_info_result chunk_list_address_info(void* address) LIBTORRENT_EXPORT;

} // namespace torrent

#endif
