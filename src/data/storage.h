// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_STORAGE_H
#define LIBTORRENT_STORAGE_H

#include <vector>
#include <inttypes.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"
#include "storage_consolidator.h"
#include "storage_file.h"

namespace torrent {

// TODO: Consider making this do private inheritance of consolidator.

class Storage {
public:
  typedef std::vector<StorageFile>        FileList;
  typedef algo::RefAnchored<StorageChunk> Chunk;
  typedef algo::RefAnchor<StorageChunk>   Anchor;

  ~Storage() { clear(); }

  void                 sync()                                  { return m_consolidator.sync(); }
  void                 clear()                                 { m_anchors.clear(); m_consolidator.clear(); }

  // Call this when all files have been added.
  void                 set_chunk_size(uint32_t s);
  
  off_t                get_bytes_size()                        { return m_consolidator.get_bytes_size(); }
  uint32_t             get_chunk_total()                       { return m_consolidator.get_chunk_total(); }
  uint32_t             get_chunk_size()                        { return m_consolidator.get_chunk_size(); }

  // Make sure the protection includes read even if you only want
  // write, this ensure that we don't get a reallocation cycle if
  // others want just read.
  Chunk                get_chunk(uint32_t b, int prot);

  StorageConsolidator& get_consolidator()                      { return m_consolidator; }

private:
  StorageConsolidator  m_consolidator;
  std::vector<Anchor>  m_anchors;
};

}

#endif
  
