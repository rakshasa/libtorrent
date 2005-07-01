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

#include "config.h"

#include <algorithm>
#include <rak/functional.h>

#include "ranges.h"

namespace torrent {

void
Ranges::insert(value_type r) {
  iterator a = std::find_if(Base::rbegin(), Base::rend(),
			    rak::greater_equal(r.first, rak::mem_ptr_ref(&value_type::first))).base();

  iterator b = std::find_if(Base::begin(), Base::end(),
			    rak::less_equal(r.second, rak::mem_ptr_ref(&value_type::second)));

  // Check if the range is contained inside a single preexisting range.
  if (a != Base::rend().base() && b != Base::end() && --iterator(a) == b)
    return;

  // Erase elements in between begin and end, then update iterator a and b.
  b = Base::erase(a, b);

  // Check if the range spans all the previous ranges.
  if (Base::empty()) {
    Base::push_back(value_type(r.first, r.second));
    return;
  }

  // Add dummy elements if we're at the end of either side.
  if (b == Base::end())
    b = Base::insert(Base::end(), value_type(r.first, r.second));

  else if (b == Base::begin())
    b = ++Base::insert(Base::begin(), value_type(r.first, r.second));

  a = --iterator(b);

  // Does not touch the a and b ranges, create a new.
  if (r.first > a->second && r.second < b->first) {
    Base::insert(b, value_type(r.first, r.second));
    return;

  } else if (r.first <= a->second) {
    a->second = r.second;

  } else if (r.second >= b->first) {
    b->first = r.first;
  }
  
  // If there's no gap, remove one of the elements.
  if (a->second >= b->first) {
    a->second = b->second;
    Base::erase(b);
  }
}

void
Ranges::erase(value_type r) {
  iterator a = std::find_if(Base::rbegin(), Base::rend(),
			    rak::greater(r.first, rak::mem_ptr_ref(&value_type::first))).base();

  iterator b = std::find_if(Base::begin(), Base::end(),
			    rak::less(r.second, rak::mem_ptr_ref(&value_type::second)));

  // Erase elements in between begin and end, then update iterator a and b.
  b = Base::erase(a, b);

  if (b != Base::end() && b->first < r.second)
    b->first = r.second;

  if (b != Base::begin() && (--b)->second > r.first)
    b->second = r.first;
}

Ranges::Base::iterator
Ranges::find(uint32_t index) {
  return std::find_if(Base::begin(), Base::end(),
		      rak::less(index, rak::mem_ptr_ref(&value_type::second)));
}

bool
Ranges::has(uint32_t index) {
  Base::const_iterator itr = std::find_if(Base::begin(), Base::end(),
					  rak::less(index, rak::mem_ptr_ref(&value_type::second)));

  return itr != Base::end() && index >= itr->first;
}

Ranges&
Ranges::intersect(Ranges& r) {
  std::for_each(r.begin(), r.end(), std::bind1st(std::mem_fun(&Ranges::erase), this));

  return *this;
}

}
