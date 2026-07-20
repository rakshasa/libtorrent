#ifndef LIBTORRENT_UTILS_FUNCTIONAL_H
#define LIBTORRENT_UTILS_FUNCTIONAL_H

#include <iterator>
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

//
// Count the number of elements from the start of the containers to the first inequal element.
//

template <typename _Value>
struct compare_base {
  bool operator () (const _Value& complete, const _Value& base) const {
    return !complete.compare(0, base.size(), base);
  }
};

template <typename _InputIter1, typename _InputIter2>
typename std::iterator_traits<_InputIter1>::difference_type
count_base(_InputIter1 __first1, _InputIter1 __last1,
	   _InputIter2 __first2, _InputIter2 __last2) {

  typename std::iterator_traits<_InputIter1>::difference_type __n = 0;

  for ( ;__first1 != __last1 && __first2 != __last2; ++__first1, ++__first2, ++__n)
    if (*__first1 != *__first2)
      return __n;

  return __n;
}

template <typename _Return, typename _InputIter, typename _Ftor>
_Return
make_base(_InputIter __first, _InputIter __last, _Ftor __ftor) {
  if (__first == __last)
    return "";

  _Return __base = __ftor(*__first++);

  for ( ;__first != __last; ++__first) {
    auto __pos = count_base(__base.begin(), __base.end(),
                            __ftor(*__first).begin(), __ftor(*__first).end());

    if (__pos < (typename std::iterator_traits<_InputIter>::difference_type)__base.size())
      __base.resize(__pos);
  }

  return __base;
}

} // namespace utils

#endif
