#ifndef LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H
#define LIBTORRENT_TORRENT_UTILS_LINKED_UNORDERED_SET_H

#include <unordered_set>
#include <list>
#include <optional>
#include <string>
#include <utility>

namespace torrent::utils {

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
  void                try_rebalance_after_erase();

  // TODO: Optimize this by storing the list node pointers in the unorderd_map.

  using list_type = std::list<Key>;
  using map_type  = std::unordered_map<Key, typename list_type::iterator>;

  list_type           m_order_list;
  map_type            m_lookup_map;

  unsigned int        m_under_threshold_count{};
};

template <typename Key> inline bool       linked_unordered_set<Key>::empty() const { return m_lookup_map.empty(); }
template <typename Key> inline size_t     linked_unordered_set<Key>::size() const  { return m_lookup_map.size(); }
template <typename Key> inline const Key& linked_unordered_set<Key>::front() const { return m_order_list.front(); }

template <typename Key>
inline void
linked_unordered_set<Key>::clear() {
  m_order_list.clear();
  m_lookup_map.clear();

  m_under_threshold_count = 0;
}

template <typename Key>
inline bool
linked_unordered_set<Key>::insert_back(const Key& key) {
  auto [itr, inserted] = m_lookup_map.try_emplace(key, m_order_list.end());

  if (!inserted)
    return false;

  m_order_list.push_back(key);
  itr->second = std::prev(m_order_list.end());

  return true;
}

template <typename Key>
inline Key
linked_unordered_set<Key>::pop_front() {
  auto front_key = std::move(m_order_list.front());

  m_order_list.pop_front();
  m_lookup_map.erase(front_key);

  try_rebalance_after_erase();

  return front_key;
}

template <typename Key>
inline bool
linked_unordered_set<Key>::erase(const Key& key) {
  auto set_itr = m_lookup_map.find(key);

  if (set_itr == m_lookup_map.end())
    return false;

  m_order_list.erase(set_itr->second);
  m_lookup_map.erase(set_itr);

  try_rebalance_after_erase();

  return true;
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
