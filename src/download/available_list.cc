// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include <stdlib.h>
#include <algorithm>
#include <iterator>

#include "torrent/exceptions.h"
#include "available_list.h"

namespace torrent {

AvailableList::value_type
AvailableList::pop_random() {
  if (empty())
    throw internal_error("AvailableList::pop_random() called on an empty container");

  size_type idx = random() % size();

  value_type tmp = *(begin() + idx);
  *(begin() + idx) = back();

  pop_back();

  return tmp;
}

void
AvailableList::push_back(const SocketAddress& sa) {
  if (std::find(begin(), end(), sa) != end())
    return;

  Base::push_back(sa);
}

void
AvailableList::insert(AddressList* l) {
  if (size() > m_maxSize)
    return;

  std::sort(begin(), end());

  // Can i use use the std::remove* semantics for this, and just copy
  // to 'l'?.
  //
  // 'l' is guaranteed to be sorted, so we can just do
  // std::set_difference.
  AddressList difference;
  std::set_difference(l->begin(), l->end(), begin(), end(), std::back_inserter(difference));

  std::copy(difference.begin(), difference.end(), std::back_inserter(*static_cast<Base*>(this)));
}

void
AvailableList::erase(const SocketAddress& sa) {
  iterator itr = std::find(begin(), end(), sa);

  if (itr != end())
    erase(itr);
}

}
