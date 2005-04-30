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

#ifndef LIBTORRENT_UTILS_THROTTLE_H
#define LIBTORRENT_UTILS_THROTTLE_H

#include <algorithm>
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

  typedef typename Base::iterator         iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::value_type       value_type;
  typedef typename Base::reference        reference;
  typedef typename Base::const_reference  const_reference;
  typedef typename Base::size_type        size_type;

  using Base::clear;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  Throttle();
  ~Throttle();

  void                sort()                        { Base::sort(ThrottleCompUsed()); }
  void                quota(uint32_t v);

  iterator            insert(const_reference t)     { m_size++; return Base::insert(begin(), t); }
  void                erase(iterator itr)           { m_size--; Base::erase(itr); }

private:
  size_type           m_size;
  uint32_t            m_quota;
};

struct ThrottlePrepare {
  ThrottlePrepare() : m_used(0) {}

  template <typename T> void operator () (const T& n) { m_used += n.get_used(); }

  uint32_t m_used;
};  

template <typename T>
struct ThrottleSet {
  ThrottleSet(int quota, int size) : m_quota(quota), m_size(size) {}

  void operator () (T& t) {
    // Check if we're low on nibbles.

    if (t.get_quota() > 0)
      m_quota -= quota_stable(t, m_quota / m_size);
    else
      m_quota -= quota_starving(t, m_quota / m_size);

    t.set_used(0);
    m_size--;

    if (t.get_quota() >= 1000)
      t.activate();
  }

  int quota_starving(T& t, int quota) {
    t.set_quota(quota);

    return quota;
  }

  int quota_stable(T& t, int quota) {
    int v = std::min(t.get_used() + 1000 - t.get_quota(), quota);

    if (v <= 0)
      return 0;

    t.set_quota(t.get_quota() + v);
    return v;
  }

  int m_quota;  
  int m_size;  
};  

template <typename T> inline void
Throttle<T>::quota(uint32_t v) {
  std::for_each(begin(), end(), ThrottleSet<T>(v, m_size));
}

}

#endif  
