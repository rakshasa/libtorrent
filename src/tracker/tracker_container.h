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

#ifndef LIBTORRENT_TRACKER_TRACKER_CONTAINER_H
#define LIBTORRENT_TRACKER_TRACKER_CONTAINER_H

#include <vector>

namespace torrent {

class TrackerBase;

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class TrackerContainer : private std::vector<std::pair<int, TrackerBase*> > {
public:
  typedef std::vector<std::pair<int, TrackerBase*> > base_type;

  using base_type::value_type;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::operator[];

  ~TrackerContainer() { clear(); }

  bool                has_enabled() const;

  void                randomize();
  void                clear();

  iterator            insert(int group, TrackerBase* t);

  iterator            promote(iterator itr);

  iterator            find(TrackerBase* tb);
  iterator            find_enabled(iterator itr);
  const_iterator      find_enabled(const_iterator itr) const;

  iterator            begin_group(int group);
  iterator            end_group(int group)                    { return begin_group(group + 1); }
  void                cycle_group(int group);
};

inline TrackerContainer::iterator
TrackerContainer::insert(int group, TrackerBase* t) {
  return base_type::insert(end_group(group), value_type(group, t));
}

}

#endif
