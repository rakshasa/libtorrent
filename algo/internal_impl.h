#ifndef ALGO_INTERNAL_IMPL_H
#define ALGO_INTERNAL_IMPL_H

namespace algo {

template <typename T>
inline T& Direct(T& t) {
  return t;
}

template <typename T>
inline T& Direct(T* t) {
  return *t;
}

// template <typename T>
// inline T& Direct(T*& t) {
//   return *t;
// }

template <typename T>
inline T* Pointer(T& t) {
  return &t;
}

template <typename T>
inline T* Pointer(T* t) {
  return t;
}

// template <typename T>
// inline T* Pointer(T*& t) {
//   return t;
// }

}

#endif // ALGO_INTERNAL_IMPL_H
