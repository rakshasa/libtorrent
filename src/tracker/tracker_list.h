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

#ifndef LIBTORRENT_TRACKER_TRACKER_LIST_H
#define LIBTORRENT_TRACKER_TRACKER_LIST_H

#include <vector>

namespace torrent {

class TrackerBase;

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class TrackerList : private std::vector<std::pair<int, TrackerBase*> > {
public:
  typedef std::vector<std::pair<int, TrackerBase*> > Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;
  using Base::empty;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::operator[];

  ~TrackerList() { clear(); }

  bool                has_enabled() const;

  void                randomize();
  void                clear();

  iterator            insert(int group, TrackerBase* t);

  iterator            promote(iterator itr);

  iterator            find(TrackerBase* tb);
  iterator            find_enabled(iterator itr);

  iterator            begin_group(int group);
  iterator            end_group(int group)                    { return begin_group(group + 1); }
  void                cycle_group(int group);
};

inline TrackerList::iterator
TrackerList::insert(int group, TrackerBase* t) {
  return Base::insert(end_group(group), value_type(group, t));
}

}

#endif
