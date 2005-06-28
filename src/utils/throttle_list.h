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
#include <list>

#include "rak/functional.h"
#include "throttle_node.h"

namespace torrent {

// I'm using a list since i want to give the user an iterator that is
// permanent. When (unless i find find a non-sorting algorithm) the
// list is sorted, the iterators stay valid.

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

  void                quota(int v);

  iterator            insert(const_reference t);
  void                erase(iterator itr)           { m_size--; Base::erase(itr); }

  size_type           size() const                  { return m_size; }
  
  int                 get_used() const;

private:
  size_type           m_size;
  int                 m_quota;
};

template <typename T> 
struct ThrottleListSetUnlimited {
  void operator () (T& n) {
    n.update_quota(T::UNLIMITED);
    n.activate();
  }
};  

template <typename T>
struct ThrottleListSetStable {
  ThrottleListSetStable(int quota, int size) : m_quota(quota), m_size(size) {}

  void operator () (T& t) {
    if (t.get_quota() <= 0)
      return;

    m_quota -= quota_stable(t, m_quota / m_size);
    m_size--;

    if (t.get_quota() >= 1024)
      t.activate();
  }

  int quota_stable(T& t, int quota) {
    int base = std::max(0, t.get_quota() - t.get_used());
    int target = std::max(2048, t.get_used() * 2);
    int delegate = std::min(quota, std::max(0, target - base));

    t.update_quota(base + delegate);
    return delegate;
  }

  int m_quota;  
  int m_size;  
};  

template <typename T>
struct ThrottleListSetStarving {
  ThrottleListSetStarving(int quota, int size) : m_quota(quota), m_size(size) {}

  void operator () (T& t) {
    if (t.get_quota() > 0)
      return;

    t.update_quota(m_quota / m_size);

    if (t.get_quota() >= 1024)
      t.activate();
  }

  int m_quota;  
  int m_size;  
};  

template <typename T> void
ThrottleList<T>::quota(int v) {
  if (v != UNLIMITED) {
    // Stable partition on starved nodes will put hungry nodes last in
    // the list over time. TODO?
    ThrottleListSetStable<T> stable(v, m_size);

    std::for_each(begin(), end(), stable);
    std::for_each(begin(), end(), ThrottleListSetStarving<T>(stable.m_quota, stable.m_size));

  } else {// if (m_quota != UNLIMITED) { // Some bug somewhere... find it please
    std::for_each(begin(), end(), ThrottleListSetUnlimited<T>());
  }

  m_quota = v;
}

template <typename T> inline typename ThrottleList<T>::iterator
ThrottleList<T>::insert(const_reference t) {
  m_size++;
  
  iterator itr = Base::insert(begin(), t);
  itr->update_quota(m_quota != UNLIMITED ? m_quota / m_size : UNLIMITED);

  return itr;
}

template <typename T> inline int
ThrottleList<T>::get_used() const {
  int used = 0;
  std::for_each(begin(), end(), rak::accumulate(used, std::mem_fun_ref(&ThrottleNode<T>::get_used)));
  return used;
}

}

#endif  
