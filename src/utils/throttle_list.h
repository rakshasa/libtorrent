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

#ifndef LIBTORRENT_UTILS_THROTTLE_LIST_H
#define LIBTORRENT_UTILS_THROTTLE_LIST_H

#include <algorithm>
#include <list>

#include "rak/functional.h"
#include "torrent/exceptions.h"
#include "throttle_node.h"

namespace torrent {

// I'm using a list since i want to give the user an iterator that is
// permanent. When (unless i find find a non-sorting algorithm) the
// list is sorted, the iterators stay valid.

// When the ThrottleList is given a quota it is distributed amongst
// the nodes in the list. First all stable nodes are given a chunk of
// the quota, which must be less than quota / size but somewhat more
// than what it used the last tick. A node is stable if it didn't
// spend its entire quota.  What is left of the quota is then
// distributed amongst the starving nodes.

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

    if (t.get_total() >= 1024)
      t.activate();
  }

  // Keeps the left-over quota from last tick and tries to allocate
  // enough to not starve. Currently sets the target at 2x what was
  // used last tick.
  //
  // The quota must be set to >0 as not to trigger the starving
  // functor.
  int quota_stable(T& t, int quota) {
    int target = std::max(2048, t.get_used() * 2);
    int delegate = std::min(quota, std::max(0, target - t.get_quota()));

    t.update_quota(t.get_quota() + delegate);

    if (t.get_quota() < 0)
      throw internal_error("ThrottleList::quota_stable(...) less than zero quota.");

    return delegate;
  }

  int m_quota;  
  int m_size;  
};  

template <typename T>
struct ThrottleListSetStarving {
  ThrottleListSetStarving(int quota, int size) :
    m_quota(std::max(1, quota / size)) {}

  void operator () (T& t) {
    if (t.get_quota() > 0)
      return;

    t.update_quota(m_quota);

    if (t.get_quota() < 0)
      throw internal_error("ThrottleList starving: less than zero quota.");

    if (t.get_quota() >= 1024)
      t.activate();
  }

  int m_quota;  
};  

template <typename T> void
ThrottleList<T>::quota(int v) {
  if (v != UNLIMITED) {
    ThrottleListSetStable<T> stable(v, m_size);

    std::for_each(begin(), end(), stable);

    if (stable.m_size != 0)
      std::for_each(begin(), end(), ThrottleListSetStarving<T>(stable.m_quota, stable.m_size));

  } else {// if (m_quota != UNLIMITED) { // Some bug somewhere... find it please
    std::for_each(begin(), end(), ThrottleListSetUnlimited<T>());
  }

  m_quota = v;
}

template <typename T> typename ThrottleList<T>::iterator
ThrottleList<T>::insert(const_reference t) {
  m_size++;
  
  iterator itr = Base::insert(begin(), t);
  itr->update_quota(m_quota != UNLIMITED ? m_quota / m_size : UNLIMITED);

  return itr;
}

template <typename T> int
ThrottleList<T>::get_used() const {
  return std::for_each(begin(), end(), rak::accumulate(0, std::mem_fun_ref(&ThrottleNode<T>::get_used))).result;
}

}

#endif  
