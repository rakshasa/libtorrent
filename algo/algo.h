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

#ifndef ALGO_ALGO_H
#define ALGO_ALGO_H

// A set of generic template algorithms. Support for zero or one
// parameter (reference and pointer) where it makes sense. Most
// template algorithms will support all.

// operation ()
// 1 template arguments - Return type
// 2 template arguments - Return type, Arg1 base class

// Some of the algorithms "start the chain" of the template operator ()'s
// implement an algorithm that allows one to define and start it anywhere.
//
// Only the starters of the chain can accept both reference and pointers as
// arguments. The rest accept only a reference. Unless i make every algorithm
// include a pointer version that converts them to reference. Yeah, implementing
// it like that.

#include <algo/common.h>
#include <algo/use.h>
#include <algo/operation.h>
#include <algo/container.h>

namespace algo {

// Stuff i haven't put anywhere else because of collisions.

template <class T, class M>
inline On<MemberVar<T, M*>, NewOn> new_on(M* T::*m) {
  return On<MemberVar<T, M*>, NewOn>(member(m), NewOn());
}

template <class T, class M>
inline On<MemberVar<T, M*>, DeleteOn> delete_on(M* T::*m) {
  return On<MemberVar<T, M*>, DeleteOn>(member(m), DeleteOn());
}

}

#endif // ALGO_ALGO_H
