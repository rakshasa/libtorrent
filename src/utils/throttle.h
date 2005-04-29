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

#ifndef LIBTORRENT_NET_THROTTLE_H
#define LIBTORRENT_NET_THROTTLE_H

#include <list>
#include "throttle_node.h"

namespace torrent {

// I'm using a list since i want to give the user an iterator that is
// permanent. When (unless i find find a non-sorting algorithm) the
// list is sorted, the iterators stay valid.

// How does the algorithm work? We seperate out those who spendt their
// quota and those that did not. Those that did not spend their entire
// quota try to get their current rate plus a small buffer. If a node
// used up the entire quota then we add to its requested quota
// depending on how much quota we got left.
//
// Sort the list by used quota in an increasing order. (Consider
// modifying depending on whetever the node used up its buffer or not)
// If we add upon seeing starving nodes, do it in a seperate loop and
// not while sorting.

struct ThrottleCompUsed {
  template <typename T> bool operator () (const T& t1, const T& t2) const { return t1.get_used() < t2.get_used(); }
};

template <typename T>
class Throttle : private std::list<T> {
public:
  typedef typename std::list<T> Base;

  using Base::value_type;
  using Base::reference;
  using Base::const_reference;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::clear;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  Throttle();
  ~Throttle();

  void                sort()                                  { Base::sort(ThrottleCompUsed()); }
  void                quota(uint32_t v);

  typename iterator   insert();//typename const_reference t)      { return Base::insert(t, begin()); }
  void                erase(typename iterator itr)            { Base::erase(itr); }

private:
  uint32_t            m_quota;
};

struct ThrottleStats {
  ThrottleStats() : m_used(0) {}

  template <typename T> void operator (const T& n) { m_used += n.get_used(); }

  uint32_t m_used;
};  

template <typename _Op> inline void
Throttle::quota(uint32_t v) {
}

}

#endif  
