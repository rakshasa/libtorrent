#ifndef LIBTORRENT_TORRENT_UTILS_SORTED_VECTOR_H
#define LIBTORRENT_TORRENT_UTILS_SORTED_VECTOR_H

#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace torrent::utils {

template <typename _Tp, typename _Compare = std::less<_Tp>>
class sorted_vector : private std::vector<_Tp> {
public:
  typedef std::vector<_Tp> Base;

  typedef typename Base::value_type value_type;
  typedef typename Base::const_reference const_reference;
  typedef typename Base::size_type size_type;
  typedef typename Base::const_iterator const_iterator;
  typedef typename Base::const_reverse_iterator const_reverse_iterator;

  explicit sorted_vector(_Compare compare = _Compare{}) : m_compare(std::move(compare)) {}

  using Base::clear;
  using Base::empty;
  using Base::size;
  using Base::reserve;

  const_reference front() const { return Base::front(); }
  const_reference back() const { return Base::back(); }
  const_iterator begin() const { return Base::begin(); }
  const_iterator end() const { return Base::end(); }
  const_reverse_iterator rbegin() const { return Base::rbegin(); }
  const_reverse_iterator rend() const { return Base::rend(); }
  const_reference operator[](size_type index) const { return Base::operator[](index); }

  const_iterator insert(value_type value) {
    if (Base::empty() || !m_compare(value, Base::back())) {
      Base::push_back(std::move(value));
      return std::prev(Base::end());
    }

    auto position = Base::end();

    while (position != Base::begin() && m_compare(value, *std::prev(position)))
      --position;

    return Base::insert(position, std::move(value));
  }

  bool insert_bounded(value_type value, size_type limit) {
    if (limit == 0 || (Base::size() >= limit && !m_compare(value, Base::back())))
      return false;

    insert(std::move(value));

    while (Base::size() > limit)
      Base::pop_back();

    return true;
  }

private:
  _Compare m_compare;
};

} // namespace torrent::utils

#endif
