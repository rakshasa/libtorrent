// rTorrent - BitTorrent client
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

#ifndef ALGO_CONTAINER_H
#define ALGO_CONTAINER_H

#include <algo/container_impl.h>

namespace algo {

// void for_each<bool pre_increment>(Iterator first, Iterator last, Ftor ftor)

template <typename Target, typename Call>
inline CallForEach<false, Target, Call>
for_each_on(Target target, Call call) {
  return CallForEach<false, Target, Call>(target, call);
}

template <typename Target, typename Call>
inline CallForEach<true, Target, Call>
for_each_pre_on(Target target, Call call) {
  return CallForEach<true, Target, Call>(target, call);
}

template <typename Target, typename Cond>
inline ContainsCond<Target, Cond>
contains(Target target, Cond cond) {
  return ContainsCond<Target, Cond>(target, cond);
}

template <typename Container, typename Element>
inline ContainedIn<Container, Element>
contained_in(Container container, Element element) {
  return ContainedIn<Container, Element>(container, element);
}

// If split is true, then the perlocated object is arg1 of Cond, and the
// contained element is arg2.
template <typename Target, typename Cond, typename Found>
inline FindIfOn<Target, Cond, Found, empty>
find_if_on(Target target, Cond cond, Found found) {
  return FindIfOn<Target, Cond, Found, empty>(target, cond, found, empty());
}

template <typename Target, typename Cond, typename Found, typename NotFound>
inline FindIfOn<Target, Cond, Found, NotFound>
find_if_on(Target target, Cond cond, Found found, NotFound notFound) {
  return FindIfOn<Target, Cond, Found, NotFound>(target, cond, found, notFound);
}

}

#endif // ALGO_CONTAINER_H
