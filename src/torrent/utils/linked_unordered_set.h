#ifndef LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H
#define LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H

#include <cstddef>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace torrent::utils {

template <typename Key>
struct linked_list_node {
  using value_type = std::pair<const Key, linked_list_node>;

  value_type* prev{};
  value_type* next{};
};

template <typename Key>
class linked_list_iterator;

template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
class linked_unordered_set {
public:
  using iterator       = linked_list_iterator<Key>;
  using const_iterator = linked_list_iterator<Key>;

  linked_unordered_set() = default;
  ~linked_unordered_set() = default;

  bool                empty() const;
  size_t              size() const;

  const_iterator      begin() const;
  const_iterator      end() const;

  const Key&          front() const;
  const Key&          back() const;

  template <typename K>
  const_iterator      find(const K& key) const;

  void                clear();

  bool                insert_back(const Key& key);

  Key                 pop_front();
  Key                 pop_back();

  bool                erase(const Key& key);
  const_iterator      erase(const_iterator pos);

private:
  linked_unordered_set(const linked_unordered_set&) = delete;
  linked_unordered_set& operator=(const linked_unordered_set&) = delete;

  using node_type  = linked_list_node<Key>;
  using map_type   = std::unordered_map<Key, node_type, Hash, KeyEqual>;
  using value_type = typename map_type::value_type;

  void                link_back(value_type* entry);
  void                unlink(const value_type* entry);

  void                rebalance_before_insert();
  void                rebalance_after_erase();

  map_type            m_lookup_map;

  value_type*         m_head{};
  value_type*         m_tail{};

  size_t              m_max_size{};
};

template <typename Key>
class linked_list_iterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type        = Key;
  using difference_type   = std::ptrdiff_t;
  using pointer           = const Key*;
  using reference         = const Key&;

  linked_list_iterator() = default;

  reference             operator*() const;
  pointer               operator->() const;

  linked_list_iterator& operator++();
  linked_list_iterator  operator++(int);

  bool                  operator==(const linked_list_iterator&) const = default;

private:
  template <typename, typename, typename>
  friend class linked_unordered_set;

  using entry_type = typename linked_list_node<Key>::value_type;

  explicit linked_list_iterator(const entry_type* entry);

  const entry_type* m_entry{};
};

template <typename Key, typename Hash, typename KeyEqual> inline bool       linked_unordered_set<Key, Hash, KeyEqual>::empty() const { return m_lookup_map.empty(); }
template <typename Key, typename Hash, typename KeyEqual> inline size_t     linked_unordered_set<Key, Hash, KeyEqual>::size() const  { return m_lookup_map.size(); }
template <typename Key, typename Hash, typename KeyEqual> inline const Key& linked_unordered_set<Key, Hash, KeyEqual>::front() const { return m_head->first; }
template <typename Key, typename Hash, typename KeyEqual> inline const Key& linked_unordered_set<Key, Hash, KeyEqual>::back() const  { return m_tail->first; }

template <typename Key, typename Hash, typename KeyEqual>
inline typename linked_unordered_set<Key, Hash, KeyEqual>::const_iterator
linked_unordered_set<Key, Hash, KeyEqual>::begin() const {
  return const_iterator(m_head);
}

template <typename Key, typename Hash, typename KeyEqual>
inline typename linked_unordered_set<Key, Hash, KeyEqual>::const_iterator
linked_unordered_set<Key, Hash, KeyEqual>::end() const {
  return const_iterator();
}

template <typename Key, typename Hash, typename KeyEqual>
template <typename K>
inline typename linked_unordered_set<Key, Hash, KeyEqual>::const_iterator
linked_unordered_set<Key, Hash, KeyEqual>::find(const K& key) const {
  auto itr = m_lookup_map.find(key);

  if (itr == m_lookup_map.end())
    return end();

  return const_iterator(&*itr);
}

template <typename Key, typename Hash, typename KeyEqual>
inline void
linked_unordered_set<Key, Hash, KeyEqual>::clear() {
  m_lookup_map.clear();

  m_head = nullptr;
  m_tail = nullptr;

  rebalance_after_erase();
}

template <typename Key, typename Hash, typename KeyEqual>
inline bool
linked_unordered_set<Key, Hash, KeyEqual>::insert_back(const Key& key) {
  rebalance_before_insert();

  auto [itr, inserted] = m_lookup_map.try_emplace(key);

  if (!inserted)
    return false;

  link_back(&*itr);

  return true;
}

template <typename Key, typename Hash, typename KeyEqual>
inline Key
linked_unordered_set<Key, Hash, KeyEqual>::pop_front() {
  Key front_key = m_head->first;

  unlink(m_head);
  m_lookup_map.erase(front_key);

  rebalance_after_erase();

  return front_key;
}

template <typename Key, typename Hash, typename KeyEqual>
inline Key
linked_unordered_set<Key, Hash, KeyEqual>::pop_back() {
  Key back_key = m_tail->first;

  unlink(m_tail);
  m_lookup_map.erase(back_key);

  rebalance_after_erase();

  return back_key;
}

template <typename Key, typename Hash, typename KeyEqual>
inline bool
linked_unordered_set<Key, Hash, KeyEqual>::erase(const Key& key) {
  auto itr = m_lookup_map.find(key);

  if (itr == m_lookup_map.end())
    return false;

  unlink(&*itr);
  m_lookup_map.erase(itr);

  rebalance_after_erase();

  return true;
}

template <typename Key, typename Hash, typename KeyEqual>
inline typename linked_unordered_set<Key, Hash, KeyEqual>::const_iterator
linked_unordered_set<Key, Hash, KeyEqual>::erase(const_iterator pos) {
  auto next = const_iterator(pos.m_entry->second.next);

  unlink(pos.m_entry);
  m_lookup_map.erase(m_lookup_map.find(pos.m_entry->first));

  rebalance_after_erase();

  return next;
}

template <typename Key, typename Hash, typename KeyEqual>
inline void
linked_unordered_set<Key, Hash, KeyEqual>::link_back(value_type* entry) {
  entry->second.prev = m_tail;
  entry->second.next = nullptr;

  if (m_tail != nullptr)
    m_tail->second.next = entry;
  else
    m_head = entry;

  m_tail = entry;
}

template <typename Key, typename Hash, typename KeyEqual>
inline void
linked_unordered_set<Key, Hash, KeyEqual>::unlink(const value_type* entry) {
  if (entry->second.prev != nullptr)
    entry->second.prev->second.next = entry->second.next;
  else
    m_head = entry->second.next;

  if (entry->second.next != nullptr)
    entry->second.next->second.prev = entry->second.prev;
  else
    m_tail = entry->second.prev;
}

template <typename Key, typename Hash, typename KeyEqual>
inline void
linked_unordered_set<Key, Hash, KeyEqual>::rebalance_before_insert() {
  if (m_lookup_map.size() != 0) {
    if (m_lookup_map.size() > m_max_size)
      m_max_size = m_lookup_map.size();

    return;
  }

  if (m_max_size == 0)
    return;

  m_lookup_map.reserve(m_max_size);
  m_max_size = 0;
}

template <typename Key, typename Hash, typename KeyEqual>
inline void
linked_unordered_set<Key, Hash, KeyEqual>::rebalance_after_erase() {
  if (!m_lookup_map.empty())
    return;

  map_type empty_map(0, m_lookup_map.hash_function(), m_lookup_map.key_eq());
  m_lookup_map.swap(empty_map);
}

template <typename Key>
inline linked_list_iterator<Key>::linked_list_iterator(const entry_type* entry) :
    m_entry(entry) {}

template <typename Key>
inline const Key&
linked_list_iterator<Key>::operator*() const {
  return m_entry->first;
}

template <typename Key>
inline const Key*
linked_list_iterator<Key>::operator->() const {
  return &m_entry->first;
}

template <typename Key>
inline linked_list_iterator<Key>&
linked_list_iterator<Key>::operator++() {
  m_entry = m_entry->second.next;
  return *this;
}

template <typename Key>
inline linked_list_iterator<Key>
linked_list_iterator<Key>::operator++(int) {
  auto itr = *this;
  ++*this;
  return itr;
}

} // namespace torrent::utils

#endif
