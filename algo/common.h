#ifndef ALGO_COMMON_H
#define ALGO_COMMON_H

#include <algo/common_impl.h>

namespace algo {

// empty() is a structure

// back_as_ref() is a structure

// back_as_ptr() is a structure

template <typename Ret, typename Ftor>
inline Convert<Ret, Ftor> convert(Ftor ftor) {
  return Convert<Ret, Ftor>(ftor);
}

template <typename Ret, typename Ftor>
inline ConvertPtr<Ret, Ftor> convert_ptr(Ftor ftor) {
  return ConvertPtr<Ret, Ftor>(ftor);
}

// Be carefull when using this, make sure you get the reference to the pointer
// that new should be called on.
inline NewOn new_on() {
  return NewOn();
}

inline DeleteOn delete_on() {
  return DeleteOn();
}

template <typename Cond, typename Then>
inline IfOn<Cond, Then, empty> if_on(Cond c, Then t) {
  return IfOn<Cond, Then, empty>(c, t, empty());
}

template <typename Cond, typename Then, typename Else>
inline IfOn<Cond, Then, Else> if_on(Cond c, Then t, Else e) {
  return IfOn<Cond, Then, Else>(c, t, e);
}

template <typename Ret, typename Cond, typename Then>
inline Convert<Ret, IfOn<Cond, Then, empty> > if_on(Cond c, Then t) {
  return convert<Ret>(IfOn<Cond, Then, empty>(c, t, empty()));
}

template <typename Ret, typename Cond, typename Then, typename Else>
inline Convert<Ret, IfOn<Cond, Then, Else> > if_on(Cond c, Then t, Else e) {
  return convert<Ret>(IfOn<Cond, Then, Else>(c, t, e));
}

template <typename Ftor1, typename Ftor2>
inline Branch<Ftor1, Ftor2> branch(Ftor1 ftor1, Ftor2 ftor2) {
  return Branch<Ftor1, Ftor2>(ftor1, ftor2);
}

// Using branch<type>(...) will return the result of ftor2.
template <typename Ret, typename Ftor1, typename Ftor2>
inline Convert<Ret, Branch<Ftor1, Ftor2> > branch(Ftor1 ftor1, Ftor2 ftor2) {
  return convert<Ret>(Branch<Ftor1, Ftor2>(ftor1, ftor2));
}

}

#endif // ALGO_COMMON_H
