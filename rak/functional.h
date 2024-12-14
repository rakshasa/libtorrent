#ifndef RAK_FUNCTIONAL_H
#define RAK_FUNCTIONAL_H

#include <utility>

namespace rak {

template <typename Container>
inline void
slot_list_call(const Container& slot_list) {
  if (slot_list.empty())
    return;

  typename Container::const_iterator first = slot_list.begin();
  typename Container::const_iterator next  = slot_list.begin();

  while (++next != slot_list.end()) {
    (*first)();
    first = next;
  }

  (*first)();
}

template <typename Container, typename Arg1>
inline void
slot_list_call(const Container& slot_list, Arg1 arg1) {
  if (slot_list.empty())
    return;

  typename Container::const_iterator first = slot_list.begin();
  typename Container::const_iterator next  = slot_list.begin();

  while (++next != slot_list.end()) {
    (*first)(arg1);
    first = next;
  }
}

} // namespace rak

#endif
