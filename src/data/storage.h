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

#ifndef LIBTORRENT_STORAGE_H
#define LIBTORRENT_STORAGE_H

#include <vector>
#include <inttypes.h>

#include "storage_chunk.h"
#include "storage_consolidator.h"
#include "storage_file.h"
#include "utils/ref_anchored.h"

namespace torrent {

// TODO: Consider making this do private inheritance of consolidator.

class Storage {
public:
  typedef std::vector<StorageFile>  FileList;
  typedef RefAnchored<StorageChunk> Chunk;
  typedef RefAnchor<StorageChunk>   Anchor;

  ~Storage() { clear(); }

  void                 sync()                                  { return m_consolidator.sync(); }
  void                 clear()                                 { m_anchors.clear(); m_consolidator.clear(); }

  // Call this when all files have been added.
  void                 set_chunk_size(uint32_t s);
  
  off_t                get_bytes_size() const                  { return m_consolidator.get_bytes_size(); }
  uint32_t             get_chunk_total() const                 { return m_consolidator.get_chunk_total(); }
  uint32_t             get_chunk_size() const                  { return m_consolidator.get_chunk_size(); }

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
  
