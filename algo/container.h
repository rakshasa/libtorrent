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
