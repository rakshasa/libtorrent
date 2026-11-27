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

  bool empty(size_t index) const { return m_lists[index].empty(); }
  size_t size(size_t index) const { return m_lists[index].size(); }

  iterator begin(size_t index) { return m_lists[index].begin(); }
  iterator end(size_t index) { return m_lists[index].end(); }
  const_iterator begin(size_t index) const { return m_lists[index].begin(); }
  const_iterator end(size_t index) const { return m_lists[index].end(); }

  T& front(size_t index) { return m_lists[index].front(); }
  const T& front(size_t index) const { return m_lists[index].front(); }
  T& back(size_t index) { return m_lists[index].back(); }
  const T& back(size_t index) const { return m_lists[index].back(); }

  void push_back(size_t index, const T& value) { m_lists[index].push_back(value); }
  void push_back(size_t index, T&& value) { m_lists[index].push_back(std::move(value)); }
  void push_front(size_t index, const T& value) { m_lists[index].push_front(value); }
  void push_front(size_t index, T&& value) { m_lists[index].push_front(std::move(value)); }
  template <typename... Args>
  T& emplace_back(size_t index, Args&&... args) { return m_lists[index].emplace_back(std::forward<Args>(args)...); }
  template <typename... Args>
  T& emplace_front(size_t index, Args&&... args) { return m_lists[index].emplace_front(std::forward<Args>(args)...); }
  template <typename... Args>
  iterator emplace(size_t index, const_iterator pos, Args&&... args) { return m_lists[index].emplace(pos, std::forward<Args>(args)...); }
  iterator insert(size_t index, const_iterator pos, const T& value) { return m_lists[index].insert(pos, value); }
  iterator insert(size_t index, const_iterator pos, T&& value) { return m_lists[index].insert(pos, std::move(value)); }
  void pop_front(size_t index) { m_lists[index].pop_front(); }
  void pop_back(size_t index) { m_lists[index].pop_back(); }
  iterator erase(size_t index, const_iterator pos) { return m_lists[index].erase(pos); }
  iterator erase(size_t index, const_iterator first, const_iterator last) { return m_lists[index].erase(first, last); }
  void clear(size_t index) { m_lists[index].clear(); }
  void clear() { for (auto& list : m_lists) list.clear(); }

private:
  std::array<list_type, Size> m_lists;
};

} // namespace torrent::utils

#endif
