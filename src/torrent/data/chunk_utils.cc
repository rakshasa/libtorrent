#include "config.h"

#include "chunk_utils.h"
#include "download.h"

#include "manager.h"

#include "data/chunk_manager.h"
#include "download/download_wrapper.h"
#include "torrent/download/download_manager.h"

#include "data/chunk_list.h"
#include "data/file.h"

namespace torrent {

std::vector<vm_mapping>
chunk_list_mapping(Download* download) {
  ChunkList* chunk_list = download->ptr()->main()->chunk_list();

  std::vector<vm_mapping> mappings;

  for (const auto& chunk : *chunk_list) {
    if (!chunk.is_valid())
      continue;

    for (const auto& itr2 : *chunk.chunk()) {
      if (itr2.mapped() != ChunkPart::MAPPED_MMAP)
        continue;

      vm_mapping val = {itr2.chunk().ptr(), itr2.chunk().size_aligned()};
      mappings.push_back(val);
    }
  }

  return mappings;
}

chunk_info_result
chunk_list_address_info(void* address) {
  for (const auto& chunk : *manager->chunk_manager()) {
    auto result = chunk->find_address(address);

    if (result.first != chunk->end()) {
      auto d_itr = manager->download_manager()->find_chunk_list(chunk);

      if (d_itr == manager->download_manager()->end())
        return chunk_info_result();

      chunk_info_result ci;
      ci.download = Download(*d_itr);
      ci.chunk_index = result.first->index();
      ci.chunk_offset = result.second->position() +
                        std::distance(result.second->chunk().begin(), static_cast<char*>(address));

      ci.file_path = result.second->file()->frozen_path().c_str();
      ci.file_offset = result.second->file_offset() +
                       std::distance(result.second->chunk().begin(), static_cast<char*>(address));

      return ci;
    }
  }

  return chunk_info_result();
}

} // namespace torrent
