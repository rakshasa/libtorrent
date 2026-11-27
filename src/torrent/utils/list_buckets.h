#ifndef LIBTORRENT_TORRENT_UTILS_LIST_BUCKETS_H
#define LIBTORRENT_TORRENT_UTILS_LIST_BUCKETS_H

#include <array>
#include <cstddef>
#include <list>
#include <utility>

namespace torrent::utils {

template <typename T, unsigned int Size>
class list_buckets {
public:
  using list_type      = std::list<T>;
  using iterator       = typename list_type::iterator;
  using const_iterator = typename list_type::const_iterator;

  bool                empty(unsigned int index) const;
  unsigned int        size(unsigned int index) const;

  iterator            begin(unsigned int index);
  iterator            end(unsigned int index);
  const_iterator      begin(unsigned int index) const;
  const_iterator      end(unsigned int index) const;

  T&                  front(unsigned int index);
  const T&            front(unsigned int index) const;
  T&                  back(unsigned int index);
  const T&            back(unsigned int index) const;

  void                push_back(unsigned int index, const T& value);
  void                push_back(unsigned int index, T&& value);
  void                push_front(unsigned int index, const T& value);
  void                push_front(unsigned int index, T&& value);

  template <typename... Args>
  T&                  emplace_back(unsigned int index, Args&&... args);

  template <typename... Args>
  T&                  emplace_front(unsigned int index, Args&&... args);

  template <typename... Args>
  iterator            emplace(unsigned int index, const_iterator pos, Args&&... args);

  iterator            insert(unsigned int index, const_iterator pos, const T& value);
  iterator            insert(unsigned int index, const_iterator pos, T&& value);
  void                pop_front(unsigned int index);
  void                pop_back(unsigned int index);
  iterator            erase(unsigned int index, const_iterator pos);
  iterator            erase(unsigned int index, const_iterator first, const_iterator last);
  void                clear(unsigned int index);
  void                clear();

private:
  std::array<list_type, Size> m_lists;
};

template <typename T, unsigned int Size>
inline bool
list_buckets<T, Size>::empty(unsigned int index) const {
  return m_lists[index].empty();
}

template <typename T, unsigned int Size>
inline unsigned int
list_buckets<T, Size>::size(unsigned int index) const {
  return m_lists[index].size();
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::begin(unsigned int index) {
  return m_lists[index].begin();
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::end(unsigned int index) {
  return m_lists[index].end();
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::const_iterator
list_buckets<T, Size>::begin(unsigned int index) const {
  return m_lists[index].begin();
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::const_iterator
list_buckets<T, Size>::end(unsigned int index) const {
  return m_lists[index].end();
}

template <typename T, unsigned int Size>
inline T&
list_buckets<T, Size>::front(unsigned int index) {
  return m_lists[index].front();
}

template <typename T, unsigned int Size>
inline const T&
list_buckets<T, Size>::front(unsigned int index) const {
  return m_lists[index].front();
}

template <typename T, unsigned int Size>
inline T&
list_buckets<T, Size>::back(unsigned int index) {
  return m_lists[index].back();
}

template <typename T, unsigned int Size>
inline const T&
list_buckets<T, Size>::back(unsigned int index) const {
  return m_lists[index].back();
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::push_back(unsigned int index, const T& value) {
  m_lists[index].push_back(value);
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::push_back(unsigned int index, T&& value) {
  m_lists[index].push_back(std::move(value));
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::push_front(unsigned int index, const T& value) {
  m_lists[index].push_front(value);
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::push_front(unsigned int index, T&& value) {
  m_lists[index].push_front(std::move(value));
}

template <typename T, unsigned int Size>
template <typename... Args>
inline T&
list_buckets<T, Size>::emplace_back(unsigned int index, Args&&... args) {
  return m_lists[index].emplace_back(std::forward<Args>(args)...);
}

template <typename T, unsigned int Size>
template <typename... Args>
inline T&
list_buckets<T, Size>::emplace_front(unsigned int index, Args&&... args) {
  return m_lists[index].emplace_front(std::forward<Args>(args)...);
}

template <typename T, unsigned int Size>
template <typename... Args>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::emplace(unsigned int index, const_iterator pos, Args&&... args) {
  return m_lists[index].emplace(pos, std::forward<Args>(args)...);
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::insert(unsigned int index, const_iterator pos, const T& value) {
  return m_lists[index].insert(pos, value);
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::insert(unsigned int index, const_iterator pos, T&& value) {
  return m_lists[index].insert(pos, std::move(value));
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::pop_front(unsigned int index) {
  m_lists[index].pop_front();
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::pop_back(unsigned int index) {
  m_lists[index].pop_back();
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::erase(unsigned int index, const_iterator pos) {
  return m_lists[index].erase(pos);
}

template <typename T, unsigned int Size>
inline typename list_buckets<T, Size>::iterator
list_buckets<T, Size>::erase(unsigned int index, const_iterator first, const_iterator last) {
  return m_lists[index].erase(first, last);
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::clear(unsigned int index) {
  m_lists[index].clear();
}

template <typename T, unsigned int Size>
inline void
list_buckets<T, Size>::clear() {
  for (auto& list : m_lists)
    list.clear();
}

} // namespace torrent::utils

#endif
