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

#ifndef LIBTORRENT_STORAGE_CHUNK_H
#define LIBTORRENT_STORAGE_CHUNK_H

#include <vector>
#include "file_chunk.h"

namespace torrent {

class StorageChunk {
public:
  friend class StorageConsolidator;

  struct Node {
    Node(unsigned int pos, unsigned int len) : position(pos), length(len) {}

    FileChunk    chunk;
    unsigned int position;
    unsigned int length;
  };

  typedef std::vector<Node*> Nodes;

  StorageChunk(unsigned int index) : m_index(index), m_size(0) {}
  ~StorageChunk()                              { clear(); }

  bool is_valid();

  int          get_index()                     { return m_index; }
  unsigned int get_size()                      { return m_size; }
  Nodes&       get_nodes()                     { return m_nodes; }

  // Get the Node that contains 'pos'.
  Node&        get_position(unsigned int pos);

  void clear();

protected:
  // When adding file chunks, make sure you clear this object if *any*
  // fails.
  FileChunk&   add_file(unsigned int length);

private:
  StorageChunk(const StorageChunk&);
  void operator = (const StorageChunk&);
  
  unsigned int m_index;
  unsigned int m_size;
  Nodes        m_nodes;
};

}

#endif

  
