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
#include "storage_node.h"

namespace torrent {

class StorageChunk : private std::vector<StorageNode> {
public:
  typedef std::vector<StorageNode> Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  StorageChunk(uint32_t index) : m_index(index), m_size(0) {}
  ~StorageChunk()                                           { clear(); }

  bool                is_valid();

  int                 get_index()                           { return m_index; }
  uint32_t            get_size()                            { return m_size; }

  // Get the Node that contains 'pos'.
  iterator            at_position(uint32_t pos);

  void                clear();

  void                push_back(const MemoryChunk& c);

private:
  StorageChunk(const StorageChunk&);
  void operator = (const StorageChunk&);
  
  uint32_t            m_index;
  uint32_t            m_size;
};

}

#endif

  
