#ifndef LIBTORRENT_UTILS_FUNCTIONAL_H
#define LIBTORRENT_UTILS_FUNCTIONAL_H

#include <tuple>
#include <utility>

namespace utils {

// Container must be a list as the entries may be removed during the call.
template <typename Container, typename... Args>
inline void
slot_list_call(const Container& slot_list, Args&&... args) {
  if (slot_list.empty())
    return;

  auto first = slot_list.begin();
  auto next  = slot_list.begin();

  auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

  while (++next != slot_list.end()) {
    std::apply(*first, args_tuple);
    first = next;
  }

  std::apply(*first, args_tuple);
}

} // namespace utils

#endif
