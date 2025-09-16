#ifndef RAK_UNORDERED_VECTOR_H
#define RAK_UNORDERED_VECTOR_H

#include <vector>

namespace rak {

template <typename _Tp>
class unordered_vector : private std::vector<_Tp> {
public:
  typedef std::vector<_Tp> Base;

  typedef typename Base::value_type value_type;
  typedef typename Base::pointer pointer;
  typedef typename Base::const_pointer const_pointer;
  typedef typename Base::reference reference;
  typedef typename Base::const_reference const_reference;
  typedef typename Base::size_type size_type;
  typedef typename Base::difference_type difference_type;
  typedef typename Base::allocator_type allocator_type;

  typedef typename Base::iterator iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::const_iterator const_iterator;
  typedef typename Base::const_reverse_iterator const_reverse_iterator;

  using Base::clear;
  using Base::empty;
  using Base::size;
  using Base::reserve;

  using Base::front;
  using Base::back;
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::push_back;
  using Base::pop_back;

  // Use the range erase function, the single element erase gets
  // overloaded.
  using Base::erase;

  iterator            erase(iterator position);

private:
};

template <typename _Tp>
typename unordered_vector<_Tp>::iterator
unordered_vector<_Tp>::erase(iterator position) {
  // We don't need to check if position == end - 1 since we then copy
  // to the position we pop later.
  *position = std::move(Base::back());
  Base::pop_back();

  return position;
}

}

#endif
