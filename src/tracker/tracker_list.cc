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

#include <algo/algo.h>

#include "torrent/exceptions.h"

#include "tracker_http.h"
#include "tracker_list.h"

namespace torrent {

void
TrackerList::randomize() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group(itr->first);

    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

void
TrackerList::clear() {
  std::for_each(begin(), end(), algo::delete_on(&value_type::second));

  Base::clear();
}

void
TrackerList::promote(iterator itr) {
  iterator beg = begin_group(itr->first);

  if (beg == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group");

  // GCC 3.3 bug, don't use yet.
  //std::swap(beg, itr);

  value_type tmp = *beg;
  *beg = *itr;
  *itr = tmp;
}

TrackerList::iterator
TrackerList::begin_group(int group) {
  return std::find_if(begin(), end(), algo::leq(algo::value(group), algo::member(&value_type::first)));
}

}
