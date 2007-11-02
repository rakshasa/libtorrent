// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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
  return std::find_if(begin(), end(), std::mem_fun(&TrackerBase::is_enabled)) != end();
}

void
TrackerContainer::randomize() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group((*itr)->group());
    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

void
TrackerContainer::clear() {
  std::for_each(begin(), end(), rak::call_delete<TrackerBase>());
  base_type::clear();
}

TrackerContainer::iterator
TrackerContainer::promote(iterator itr) {
  iterator first = begin_group((*itr)->group());

  if (first == end())
    throw internal_error("torrent::TrackerContainer::promote(...) Could not find beginning of group.");

  std::swap(first, itr);
  return first;
}

TrackerContainer::iterator
TrackerContainer::find_enabled(iterator itr) {
  while (itr != end() && !(*itr)->is_enabled())
    ++itr;

  return itr;
}

TrackerContainer::const_iterator
TrackerContainer::find_enabled(const_iterator itr) const {
  while (itr != end() && !(*itr)->is_enabled())
    ++itr;

  return itr;
}

TrackerContainer::iterator
TrackerContainer::begin_group(unsigned int group) {
  return std::find_if(begin(), end(), rak::less_equal(group, std::mem_fun(&TrackerBase::group)));
}

void
TrackerContainer::cycle_group(unsigned int group) {
  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || (*itr)->group() != group)
    return;

  while (++itr != end() && (*itr)->group() == group) {
    std::swap(itr, prev);
    prev = itr;
  }
}

}
