#ifndef LIBTORRENT_TORRENT_UTILS_LIMITED_VECTOR_H
#define LIBTORRENT_TORRENT_UTILS_LIMITED_VECTOR_H

#include <functional>
#include <iterator>
#include <utility>
#include <vector>

namespace torrent::utils {

// The caller must provide a nonzero limit, use insert_sorted only while below the limit, and use
// insert_bounded only when the vector is full and sorted.

template <typename _Tp, typename _Compare = std::greater<_Tp>>
class limited_vector : private std::vector<_Tp> {
public:
  typedef std::vector<_Tp> base_type;

  typedef typename base_type::value_type value_type;
  typedef typename base_type::const_reference const_reference;
  typedef typename base_type::size_type size_type;
  typedef typename base_type::const_iterator const_iterator;
  typedef typename base_type::const_reverse_iterator const_reverse_iterator;

  using base_type::clear;
  using base_type::empty;
  using base_type::size;
  using base_type::front;
  using base_type::back;
  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;
  using base_type::operator[];

  explicit limited_vector(size_type limit, _Compare compare = _Compare{});

  size_type           limit() const;

  const_iterator      insert_sorted(value_type value);
  bool                insert_bounded(value_type value);

  template <typename _InputIt>
  void                insert_sorted(_InputIt first, _InputIt last);

  base_type           move_as_vector();

  void                reduce_to_limit();

private:
  const size_type     m_limit;
  _Compare            m_compare;
};

template <typename _Tp, typename _Compare, typename _InputIt>
void copy_limited(_InputIt first, _InputIt last, limited_vector<_Tp, _Compare>& vec);

//
// Implementation:
//

template <typename _Tp, typename _Compare>
limited_vector<_Tp, _Compare>::limited_vector(size_type limit, _Compare compare)
  : m_limit(limit),
    m_compare(std::move(compare)) {

  base_type::reserve(limit);
}

template <typename _Tp, typename _Compare>
inline typename limited_vector<_Tp, _Compare>::size_type
limited_vector<_Tp, _Compare>::limit() const {
  return m_limit;
}

template <typename _Tp, typename _Compare>
inline typename limited_vector<_Tp, _Compare>::const_iterator
limited_vector<_Tp, _Compare>::insert_sorted(value_type value) {
  auto position = base_type::end();

  while (position != base_type::begin() && m_compare(value, *std::prev(position)))
    --position;

  return base_type::insert(position, std::move(value));
}

template <typename _Tp, typename _Compare>
bool
limited_vector<_Tp, _Compare>::insert_bounded(value_type value) {
  auto position = std::prev(base_type::end());

  if (!m_compare(value, *position))
    return false;

  while (position != base_type::begin() && m_compare(value, *std::prev(position))) {
    *position = std::move(*std::prev(position));
    --position;
  }

  *position = std::move(value);
  return true;
}

template <typename _Tp, typename _Compare>
template <typename _InputIt>
void
limited_vector<_Tp, _Compare>::insert_sorted(_InputIt first, _InputIt last) {
  if (first == last)
    return;

  base_type::insert(base_type::end(), first, last);
  std::sort(base_type::begin(), base_type::end(), m_compare);
}

template <typename _Tp, typename _Compare>
typename limited_vector<_Tp, _Compare>::base_type
limited_vector<_Tp, _Compare>::move_as_vector() {
  return std::move(static_cast<base_type&>(*this));
}

template <typename _Tp, typename _Compare>
void
limited_vector<_Tp, _Compare>::reduce_to_limit() {
  if (base_type::size() > m_limit)
    base_type::erase(base_type::begin() + m_limit, base_type::end());
}

template <typename _Tp, typename _Compare, typename _InputIt>
void
copy_limited(_InputIt first, _InputIt last, limited_vector<_Tp, _Compare>& vec) {
  vec.reduce_to_limit();

  auto middle = first + std::min<size_t>(vec.limit() - vec.size(), std::distance(first, last));

  vec.insert_sorted(first, middle);

  for (; middle != last; ++middle)
    vec.insert_bounded(*middle);
}

} // namespace torrent::utils

#endif
