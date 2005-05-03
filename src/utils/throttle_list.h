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

#ifndef LIBTORRENT_UTILS_THROTTLE_LIST_H
#define LIBTORRENT_UTILS_THROTTLE_LIST_H

#include <algorithm>
#include <functional>
#include <list>
#include "throttle_node.h"

namespace torrent {

// I'm using a list since i want to give the user an iterator that is
// permanent. When (unless i find find a non-sorting algorithm) the
// list is sorted, the iterators stay valid.

struct ThrottleListCompUsed {
  template <typename T> bool operator () (const T& t1, const T& t2) const { return t1.get_used() < t2.get_used(); }
};

template <typename T>
class ThrottleList : private std::list<T> {
public:
  enum {
    UNLIMITED = -1
  };

  typedef typename std::list<T> Base;

  typedef typename Base::iterator         iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::value_type       value_type;
  typedef typename Base::reference        reference;
  typedef typename Base::const_reference  const_reference;
  typedef typename Base::size_type        size_type;

  using Base::clear;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  ThrottleList() : m_size(0), m_quota(UNLIMITED) {}

  void                sort()                        { Base::sort(ThrottleListCompUsed()); }
  void                quota(int v);

  iterator            insert(const_reference t);
  void                erase(iterator itr)           { m_size--; Base::erase(itr); }

  size_type           size() const                  { return m_size; }

private:
  size_type           m_size;
  int                 m_quota;
};

template <typename T> 
struct ThrottleListSetUnlimited {
  void operator () (T& n) {
    n.set_quota(T::UNLIMITED);
    n.activate();
  }
};  

template <typename T>
struct ThrottleListSet {
  ThrottleListSet(int quota, int size) : m_quota(quota), m_size(size) {}

  void operator () (T& t) {
    // Check if we're low on nibbles.

    if (t.get_quota() > 0)
      m_quota -= quota_stable(t, m_quota / m_size);
    else
      m_quota -= quota_starving(t, m_quota / m_size);

    m_size--;

    if (t.get_quota() >= 1024)
      t.activate();
  }

  int quota_starving(T& t, int quota) {
    t.update_quota(quota);

    return quota;
  }

  // We set a target of 'used' + 1000 bytes, which will guarantee that
  // the node gradually builds up to 1000 when given very low quotas
  // each tick.
  int quota_stable(T& t, int quota) {
    int v = std::min(t.get_used() + 2 * 1024 - t.get_quota(), quota);

    if (v <= 0)
      return 0;

    t.update_quota(t.get_quota() + v);
    return v;
  }

  int m_quota;  
  int m_size;  
};  

template <typename T> inline void
ThrottleList<T>::quota(int v) {
  if (v != UNLIMITED)
    std::for_each(begin(), end(), ThrottleListSet<T>(v, m_size));

  else if (m_quota != UNLIMITED)
    std::for_each(begin(), end(), ThrottleListSetUnlimited<T>());

  m_quota = v;
}

template <typename T> inline typename ThrottleList<T>::iterator
ThrottleList<T>::insert(const_reference t) {
  m_size++;
  
  iterator itr = Base::insert(begin(), t);
  itr->update_quota(m_quota != UNLIMITED ? m_quota / m_size : UNLIMITED);

  return itr;
}

}

#endif  
