#ifndef LIBTORRENT_TORRENT_UTILS_LIMITED_VECTOR_H
#define LIBTORRENT_TORRENT_UTILS_LIMITED_VECTOR_H

#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace torrent::utils {

// The caller must provide a nonzero limit, use insert_sorted only while below
// the limit, and use insert_bounded only when the vector is full and sorted.
template <typename _Tp, typename _Compare = std::greater<_Tp>>
class limited_vector : private std::vector<_Tp> {
public:
  typedef std::vector<_Tp> Base;

  typedef typename Base::value_type value_type;
  typedef typename Base::const_reference const_reference;
  typedef typename Base::size_type size_type;
  typedef typename Base::const_iterator const_iterator;
  typedef typename Base::const_reverse_iterator const_reverse_iterator;

  explicit limited_vector(size_type limit, _Compare compare = _Compare{})
    : m_limit(limit), m_compare(std::move(compare)) {}

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
  using Base::operator[];

  size_type limit() const { return m_limit; }

  const_iterator insert_sorted(value_type value) {
    auto position = Base::end();

    while (position != Base::begin() && m_compare(value, *std::prev(position)))
      --position;

    return Base::insert(position, std::move(value));
  }

  bool insert_bounded(value_type value) {
    auto position = std::prev(Base::end());

    if (!m_compare(value, *position))
      return false;

    while (position != Base::begin() && m_compare(value, *std::prev(position))) {
      *position = std::move(*std::prev(position));
      --position;
    }

    *position = std::move(value);
    return true;
  }

private:
  const size_type m_limit;
  _Compare m_compare;
};

} // namespace torrent::utils

#endif
