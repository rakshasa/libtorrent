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

#ifndef LIBTORRENT_PRIORITY_H
#define LIBTORRENT_PRIORITY_H

#include "utils/ranges.h"

namespace torrent {

class Priority {
public:
  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Type;

  typedef Ranges::iterator         iterator;
  typedef Ranges::reverse_iterator reverse_iterator;

  // Must be added in increasing order.
  void                add(Type t, uint32_t begin, uint32_t end) { m_ranges[t].insert(begin, end); }

  void                clear()                                   { for (int i = 0; i < 3; ++i) m_ranges[i].clear(); }

  iterator            begin(Type t)                             { return m_ranges[t].begin(); }
  iterator            end(Type t)                               { return m_ranges[t].end(); }

  reverse_iterator    rbegin(Type t)                            { return m_ranges[t].rbegin(); }
  reverse_iterator    rend(Type t)                              { return m_ranges[t].rend(); }

  iterator            find(Type t, uint32_t index)              { return m_ranges[t].find(index); }

  bool                has(Type t, uint32_t index)               { return m_ranges[t].has(index); }

private:
  Ranges              m_ranges[3];
};

}

#endif
