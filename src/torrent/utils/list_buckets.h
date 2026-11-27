#ifndef LIBTORRENT_TORRENT_UTILS_LIST_BUCKETS_H
#define LIBTORRENT_TORRENT_UTILS_LIST_BUCKETS_H

#include <array>
#include <cstddef>
#include <list>
#include <utility>

namespace torrent::utils {

template <typename T, size_t Size>
class list_buckets {
public:
  using list_type = std::list<T>;
  using iterator = typename list_type::iterator;
  using const_iterator = typename list_type::const_iterator;

  bool empty(size_t index) const;
  size_t size(size_t index) const;

  iterator begin(size_t index);
  iterator end(size_t index);
  const_iterator begin(size_t index) const;
  const_iterator end(size_t index) const;

  T& front(size_t index);
  const T& front(size_t index) const;
  T& back(size_t index);
  const T& back(size_t index) const;

  void push_back(size_t index, const T& value);
  void push_back(size_t index, T&& value);
  void push_front(size_t index, const T& value);
  void push_front(size_t index, T&& value);
  template <typename... Args>
  T& emplace_back(size_t index, Args&&... args);
  template <typename... Args>
  T& emplace_front(size_t index, Args&&... args);
  template <typename... Args>
  iterator emplace(size_t index, const_iterator pos, Args&&... args);
  iterator insert(size_t index, const_iterator pos, const T& value);
  iterator insert(size_t index, const_iterator pos, T&& value);
  void pop_front(size_t index);
  void pop_back(size_t index);
  iterator erase(size_t index, const_iterator pos);
  iterator erase(size_t index, const_iterator first, const_iterator last);
  void clear(size_t index);
  void clear();

private:
  std::array<list_type, Size> m_lists;
};

template <typename T, size_t Size>
inline bool list_buckets<T, Size>::empty(size_t index) const { return m_lists[index].empty(); }

template <typename T, size_t Size>
inline size_t list_buckets<T, Size>::size(size_t index) const { return m_lists[index].size(); }

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::begin(size_t index) { return m_lists[index].begin(); }

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::end(size_t index) { return m_lists[index].end(); }

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::const_iterator list_buckets<T, Size>::begin(size_t index) const { return m_lists[index].begin(); }

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::const_iterator list_buckets<T, Size>::end(size_t index) const { return m_lists[index].end(); }

template <typename T, size_t Size>
inline T& list_buckets<T, Size>::front(size_t index) { return m_lists[index].front(); }

template <typename T, size_t Size>
inline const T& list_buckets<T, Size>::front(size_t index) const { return m_lists[index].front(); }

template <typename T, size_t Size>
inline T& list_buckets<T, Size>::back(size_t index) { return m_lists[index].back(); }

template <typename T, size_t Size>
inline const T& list_buckets<T, Size>::back(size_t index) const { return m_lists[index].back(); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::push_back(size_t index, const T& value) { m_lists[index].push_back(value); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::push_back(size_t index, T&& value) { m_lists[index].push_back(std::move(value)); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::push_front(size_t index, const T& value) { m_lists[index].push_front(value); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::push_front(size_t index, T&& value) { m_lists[index].push_front(std::move(value)); }

template <typename T, size_t Size>
template <typename... Args>
inline T& list_buckets<T, Size>::emplace_back(size_t index, Args&&... args) {
  return m_lists[index].emplace_back(std::forward<Args>(args)...);
}

template <typename T, size_t Size>
template <typename... Args>
inline T& list_buckets<T, Size>::emplace_front(size_t index, Args&&... args) {
  return m_lists[index].emplace_front(std::forward<Args>(args)...);
}

template <typename T, size_t Size>
template <typename... Args>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::emplace(size_t index, const_iterator pos, Args&&... args) {
  return m_lists[index].emplace(pos, std::forward<Args>(args)...);
}

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::insert(size_t index, const_iterator pos, const T& value) {
  return m_lists[index].insert(pos, value);
}

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::insert(size_t index, const_iterator pos, T&& value) {
  return m_lists[index].insert(pos, std::move(value));
}

template <typename T, size_t Size>
inline void list_buckets<T, Size>::pop_front(size_t index) { m_lists[index].pop_front(); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::pop_back(size_t index) { m_lists[index].pop_back(); }

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::erase(size_t index, const_iterator pos) {
  return m_lists[index].erase(pos);
}

template <typename T, size_t Size>
inline typename list_buckets<T, Size>::iterator list_buckets<T, Size>::erase(size_t index, const_iterator first, const_iterator last) {
  return m_lists[index].erase(first, last);
}

template <typename T, size_t Size>
inline void list_buckets<T, Size>::clear(size_t index) { m_lists[index].clear(); }

template <typename T, size_t Size>
inline void list_buckets<T, Size>::clear() {
  for (auto& list : m_lists)
    list.clear();
}

} // namespace torrent::utils

#endif
