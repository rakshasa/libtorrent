#ifndef RAK_FUNCTIONAL_H
#define RAK_FUNCTIONAL_H

#include <utility>

namespace rak {

template <typename Container, typename... Args>
inline void
slot_list_call(const Container& slot_list, Args&&... args) {
  for (const auto& slot : slot_list) {
    slot(std::forward<Args>(args)...);
  }
}

} // namespace rak

#endif
