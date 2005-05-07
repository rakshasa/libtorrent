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

#ifndef LIBTORRENT_UTILS_UNORDERED_VECTOR_H
#define LIBTORRENT_UTILS_UNORDERED_VECTOR_H

#include <vector>

namespace torrent {

// Consider renaming to unordered_vector when making a seperate
// utilities package.

template <typename _Tp>
class unordered_vector : private std::vector<_Tp> {
public:
  typedef std::vector<_Tp> Base;
  
  typedef typename Base::value_type value_type;
  typedef typename Base::pointer pointer;
  typedef typename Base::const_pointer const_pointer;
  typedef typename Base::reference reference;
  typedef typename Base::const_reference const_reference;
  typedef typename Base::size_type size_type;
  typedef typename Base::difference_type difference_type;
  typedef typename Base::allocator_type allocator_type;

  typedef typename Base::iterator iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::const_iterator const_iterator;
  typedef typename Base::const_reverse_iterator const_reverse_iterator;

  using Base::clear;
  using Base::empty;
  using Base::size;
  using Base::reserve;

  using Base::front;
  using Base::back;
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::push_back;

  iterator            insert(iterator position, const value_type& x);
  iterator            erase(iterator position);  

private:
};

template <typename _Tp>
typename unordered_vector<_Tp>::iterator
unordered_vector<_Tp>::insert(iterator position, const value_type& x) {
  Base::push_back(x);

  return --end();
}

template <typename _Tp>
typename unordered_vector<_Tp>::iterator
unordered_vector<_Tp>::erase(iterator position) {
  // We don't need to check if position == end - 1 since we then copy
  // to the position we pop later.
  *position = Base::back();
  Base::pop_back();

  return position;
}    

}

#endif
