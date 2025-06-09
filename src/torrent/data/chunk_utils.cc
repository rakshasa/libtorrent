// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "chunk_utils.h"
#include "download.h"

#include "manager.h"

#include "download/download_wrapper.h"
#include "torrent/chunk_manager.h"
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
