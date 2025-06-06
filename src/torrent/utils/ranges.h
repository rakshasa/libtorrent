#ifndef LIBTORRENT_UTILS_RANGES_H
#define LIBTORRENT_UTILS_RANGES_H

#include <algorithm>
#include <vector>

namespace torrent {

template <typename RangesType>
class ranges : private std::vector<std::pair<RangesType, RangesType> > {
public:
  using base_type = std::vector<std::pair<RangesType, RangesType>>;

  using bound_type = RangesType;

  using value_type       = typename base_type::value_type;
  using reference        = typename base_type::reference;
  using iterator         = typename base_type::iterator;
  using const_iterator   = typename base_type::const_iterator;
  using reverse_iterator = typename base_type::reverse_iterator;

  using base_type::clear;
  using base_type::empty;
  using base_type::size;
  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::front;
  using base_type::back;

  void                insert(bound_type first, bound_type last) { insert(std::make_pair(first, last)); }
  void                erase(bound_type first, bound_type last)  { erase(std::make_pair(first, last)); }

  void                insert(value_type r);
  void                erase(value_type r);

  // Find the first ranges that has an end greater than index.
  iterator            find(bound_type index);
  const_iterator      find(bound_type index) const;

  // Use find with no closest match.
  bool                has(bound_type index) const;

  size_t              intersect_distance(bound_type first, bound_type last) const;
  size_t              intersect_distance(value_type range) const;

  static ranges       create_union(const ranges& left, const ranges& right);
};

template <typename RangesType>
void
ranges<RangesType>::insert(value_type r) {
  if (r.first >= r.second)
    return;

  auto first = std::find_if(begin(), end(), [r](const value_type v) { return r.first <= v.second; });

  if (first == end() || r.second < first->first) {
    // The new range is before the first, after the last or between
    // two ranges.
    base_type::insert(first, r);

  } else {
    first->first = std::min(r.first, first->first);
    first->second = std::max(r.second, first->second);

    auto last = std::find_if(first, end(), [first](const value_type v) { return first->second < v.second; });

    if (last != end() && first->second >= last->first)
      first->second = (last++)->second;

    base_type::erase(first + 1, last);
  }
}

template <typename RangesType>
void
ranges<RangesType>::erase(value_type r) {
  if (r.first >= r.second)
    return;

  auto first = std::find_if(begin(), end(), [r](const value_type v) { return r.first < v.second; });
  auto last  = std::find_if(first, end(), [r](const value_type v) { return r.second < v.second; });

  if (first == end())
    return;

  if (first == last) {

    if (r.first > first->first) {
      std::swap(first->first, r.second);
      base_type::insert(first, value_type(r.second, r.first));

    } else if (r.second > first->first) {
      first->first = r.second;
    }

  } else {

    if (r.first > first->first)
      (first++)->second = r.first;
    
    if (last != end() && r.second > last->first)
      last->first = r.second;

    base_type::erase(first, last);
  }
}

// Find the first ranges that has an end greater than index.
template <typename RangesType>
inline typename ranges<RangesType>::iterator
ranges<RangesType>::find(bound_type index) {
  return std::find_if(begin(), end(), [index](const value_type v) { return index < v.second; });
}

template <typename RangesType>
inline typename ranges<RangesType>::const_iterator
ranges<RangesType>::find(bound_type index) const {
  return std::find_if(begin(), end(), [index](const value_type v) { return index < v.second; });
}

// Use find with no closest match.
template <typename RangesType>
bool
ranges<RangesType>::has(bound_type index) const {
  auto itr = find(index);

  return itr != end() && index >= itr->first;
}

template <typename RangesType>
size_t
ranges<RangesType>::intersect_distance(bound_type first, bound_type last) const {
  return intersect_distance(std::make_pair(first, last));
}

// The total length of all the extents within the bounds of 'range'.
template <typename RangesType>
size_t
ranges<RangesType>::intersect_distance(value_type range) const {
  auto first = find(range.first);

  if (first == end() || range.second <= first->first)
    return 0;

  size_t dist = std::min(range.second, first->second) - std::max(range.first, first->first);

  while (++first != end() && range.second > first->first)
    dist += std::min(range.second, first->second) - first->first;

  return dist;
}

template <typename RangesType>
ranges<RangesType>
ranges<RangesType>::create_union(const ranges& left, const ranges& right) {
  if (left.empty())
    return right;

  if (right.empty())
    return left;

  ranges result;

  auto   left_itr   = left.begin();
  auto   left_last  = left.end();
  auto   right_itr  = right.begin();
  auto   right_last = right.end();

  if (left_itr->first < right_itr->first)
    result.base_type::push_back(*left_itr++);
  else
    result.base_type::push_back(*right_itr++);

  while (left_itr != left_last && right_itr != right_last) {
    value_type next;

    if (left_itr->first < right_itr->first)
      next = *left_itr++;
    else
      next = *right_itr++;

    if (next.first <= result.back().second)
      result.back().second = std::max(next.second, result.back().second);
    else
      result.base_type::push_back(next);
  }

  // Only one of these while loops will be triggered.
  for (; left_itr != left_last; left_itr++) {
    if (left_itr->first <= result.back().second)
      result.back().second = std::max(left_itr->second, result.back().second);
    else
      result.base_type::push_back(*left_itr);
  }

  for (; right_itr != right_last; right_itr++) {
    if (right_itr->first <= result.back().second)
      result.back().second = std::max(right_itr->second, result.back().second);
    else
      result.base_type::push_back(*right_itr);
  }

  return result;
}

} // namespace torrent

#endif
