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

#include "socket_set.h"

namespace torrent {

const SocketSet::size_type SocketSet::npos;

inline void
SocketSet::_replace_with_last(size_type idx) {
  while (!base_type::empty() && base_type::back() == NULL)
    base_type::pop_back();

  if (idx >= m_table.size())
    throw internal_error("SocketSet::_replace_with_last(...) input out-of-bounds");

  // This should handle both npos and those that have already been
  // removed with the above while loop.
  if (idx >= size())
    return;

  *(begin() + idx) = base_type::back();
  _index(base_type::back()) = idx;

  base_type::pop_back();
}

void
SocketSet::prepare() {
  for (auto& socket : m_erased) {
    _replace_with_last(socket);
  }

  m_erased.clear();
}

void
SocketSet::reserve(size_t openMax) {
  m_table.resize(openMax, npos);

  base_type::reserve(openMax);
}

}
