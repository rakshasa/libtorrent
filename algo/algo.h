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
