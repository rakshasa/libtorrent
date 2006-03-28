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

#include <rak/functional.h>

#include "torrent/exceptions.h"

#include "tracker_base.h"
#include "tracker_container.h"

namespace torrent {

bool
TrackerContainer::has_enabled() const {
  return std::find_if(begin(), end(),
		      rak::on(rak::mem_ptr_ref(&value_type::second), std::mem_fun(&TrackerBase::is_enabled)))
    != end();
}

void
TrackerContainer::randomize() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group(itr->first);

    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

void
TrackerContainer::clear() {
  std::for_each(begin(), end(),
		rak::on(rak::mem_ptr_ref(&value_type::second), rak::call_delete<TrackerBase>()));

  Base::clear();
}

TrackerContainer::iterator
TrackerContainer::promote(iterator itr) {
  iterator beg = begin_group(itr->first);

  if (beg == end())
    throw internal_error("torrent::TrackerContainer::promote(...) Could not find beginning of group");

  // GCC 3.3 bug, don't use yet.
  //std::swap(beg, itr);

  value_type tmp = *beg;
  *beg = *itr;
  *itr = tmp;

  return beg;
}

TrackerContainer::iterator
TrackerContainer::find(TrackerBase* tb) {
  return std::find_if(begin(), end(), rak::equal(tb, rak::mem_ptr_ref(&value_type::second)));
}

TrackerContainer::iterator
TrackerContainer::find_enabled(iterator itr) {
  while (itr != end() && !itr->second->is_enabled())
    ++itr;

  return itr;
}

TrackerContainer::iterator
TrackerContainer::begin_group(int group) {
  return std::find_if(begin(), end(),
		      rak::less_equal(group, rak::mem_ptr_ref(&value_type::first)));
}

void
TrackerContainer::cycle_group(int group) {
  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || itr->first != group)
    return;

  while (++itr != end() && itr->first == group) {
    value_type tmp = *itr;
    *itr = *prev;
    *prev = tmp;

    prev = itr;
  }
}

}
