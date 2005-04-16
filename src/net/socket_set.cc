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

#include "config.h"

#include <algorithm>
#include <functional>

#include "socket_set.h"

namespace torrent {

void
SocketSet::prepare() {
  std::for_each(m_erased.begin(), m_erased.end(),
		std::bind1st(std::mem_fun(&SocketSet::_replace_with_last), this));

  m_erased.clear();
}

inline void
SocketSet::_replace_with_last(Index idx) {
  while (!Base::empty() && Base::back() == NULL)
    Base::pop_back();

  if ((size_t)idx >= size())
    return;

  *(begin() + idx) = Base::back();
  _index(Base::back()) = idx;

  Base::pop_back();
}

}
