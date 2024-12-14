#ifndef LIBTORRENT_UTILS_FUNCTIONAL_H
#define LIBTORRENT_UTILS_FUNCTIONAL_H

#include <utility>

namespace utils {

// Container must be a list as the entries may be removed during the call.
template <typename Container, typename... Args>
inline void
slot_list_call(const Container& slot_list, Args&&... args) {
  if (slot_list.empty())
    return;

  typename Container::const_iterator first = slot_list.begin();
  typename Container::const_iterator next  = slot_list.begin();

  while (++next != slot_list.end()) {
    (*first)(std::forward<Args>(args)...);
    first = next;
  }

  (*first)(std::forward<Args>(args)...);
}

} // namespace utils

#endif
