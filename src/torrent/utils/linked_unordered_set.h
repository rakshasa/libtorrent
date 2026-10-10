#ifndef LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H
#define LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H

#include <cstddef>
#include <unordered_map>
#include <utility>

namespace torrent::utils {

// Insertion-ordered set with O(1) insert_back, pop_front and erase.
//
// The ordering links are stored directly in the unordered_map's mapped value, so each element is a
// single allocation. Pointers to unordered_map elements remain valid across rehashing, which is
// what makes this safe.

template <typename Key>
class linked_unordered_set {
public:
  bool                empty() const;
  size_t              size() const;

  const Key&          front() const;

  void                clear();

  bool                insert_back(const Key& key);

  Key                 pop_front();

  bool                erase(const Key& key);

private:
  struct node_type;

  using map_type   = std::unordered_map<Key, node_type>;
  using value_type = typename map_type::value_type;

  // Acts as the list node; the key lives in the map's value_type::first.
  struct node_type {
    value_type* prev{};
    value_type* next{};
  };

  void                unlink(value_type* entry);
  void                try_rebalance_after_erase();

  map_type            m_lookup_map;

  value_type*         m_head{};
  value_type*         m_tail{};

  unsigned int        m_under_threshold_count{};
};

template <typename Key> inline bool       linked_unordered_set<Key>::empty() const { return m_lookup_map.empty(); }
template <typename Key> inline size_t     linked_unordered_set<Key>::size() const  { return m_lookup_map.size(); }
template <typename Key> inline const Key& linked_unordered_set<Key>::front() const { return m_head->first; }

template <typename Key>
inline void
linked_unordered_set<Key>::clear() {
  m_lookup_map.clear();

  m_head = nullptr;
  m_tail = nullptr;

  m_under_threshold_count = 0;
}

template <typename Key>
inline bool
linked_unordered_set<Key>::insert_back(const Key& key) {
  auto [itr, inserted] = m_lookup_map.try_emplace(key);

  if (!inserted)
    return false;

  value_type* entry = &*itr;

  entry->second.prev = m_tail;
  entry->second.next = nullptr;

  if (m_tail != nullptr)
    m_tail->second.next = entry;
  else
    m_head = entry;

  m_tail = entry;

  return true;
}

template <typename Key>
inline Key
linked_unordered_set<Key>::pop_front() {
  Key front_key = m_head->first;

  unlink(m_head);
  m_lookup_map.erase(front_key);

  try_rebalance_after_erase();

  return front_key;
}

template <typename Key>
inline bool
linked_unordered_set<Key>::erase(const Key& key) {
  auto itr = m_lookup_map.find(key);

  if (itr == m_lookup_map.end())
    return false;

  unlink(&*itr);
  m_lookup_map.erase(itr);

  try_rebalance_after_erase();

  return true;
}

template <typename Key>
inline void
linked_unordered_set<Key>::unlink(value_type* entry) {
  if (entry->second.prev != nullptr)
    entry->second.prev->second.next = entry->second.next;
  else
    m_head = entry->second.next;

  if (entry->second.next != nullptr)
    entry->second.next->second.prev = entry->second.prev;
  else
    m_tail = entry->second.prev;
}

// Rebalance the underlying set if we have been under a certain threshold for a number of operations.
template <typename Key>
inline void
linked_unordered_set<Key>::try_rebalance_after_erase() {
  // Bucket sizes are primes, but 32 is a good threshold to start rebalancing at.
  if (m_lookup_map.bucket_count() < 32)
    return;

  if (m_lookup_map.size() == 0) {
    m_lookup_map.clear();
    m_under_threshold_count = 0;
    return;
  }

  if (m_lookup_map.size() > m_lookup_map.bucket_count() / 4) {
    m_under_threshold_count = 0;
    return;
  }

  m_under_threshold_count++;

  if (m_under_threshold_count < m_lookup_map.bucket_count() / 6)
    return;

  m_lookup_map.reserve(m_lookup_map.size() * 2);
}

} // namespace torrent::utils

#endif
