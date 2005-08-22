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
#include "entry_list_node.h"
#include "utils/ref_anchored.h"

namespace torrent {

// TODO: Consider making this do private inheritance of consolidator.

class Storage {
public:
  typedef std::vector<EntryListNode>  FileList;
  typedef RefAnchored<StorageChunk> Chunk;
  typedef RefAnchor<StorageChunk>   Anchor;

  Storage() : m_chunkSize(1 << 16) {}
  ~Storage() { clear(); }

  void                clear()                                 { m_anchors.clear(); }

  uint32_t            size() const { return m_anchors.size(); }

  // Call this when all files have been added.
  void                set_size(uint32_t s);
  
  uint32_t            get_chunk_size() const                  { return m_chunkSize; }
  void                set_chunk_size(uint32_t cs)             { m_chunkSize = cs; }

  bool                has_anchor(uint32_t index, int prot);

  Chunk               get_anchor(uint32_t index) { return Chunk(m_anchors[index]); }
  Chunk               make_anchor(StorageChunk* chunk);

private:
  std::vector<Anchor> m_anchors;

  uint32_t            m_chunkSize;
};

}

#endif
  
