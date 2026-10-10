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

  // TODO: Add resizing of the underlying set after being under a certain threshold for n operations.

  void                push_back(const Key& key);

  Key                 pop_front();

  bool                erase(const Key& key);

private:
  void                try_rebalance_after_erase();

  using list_type = std::list<Key>;
  using set_type  = std::unordered_map<Key, typename list_type::iterator>;

  list_type           m_order_list;
  set_type            m_lookup_set;

  unsigned int        m_under_threshold_count{};
};

template <typename Key> inline bool   linked_unordered_set<Key>::empty() const { return m_lookup_set.empty(); }
template <typename Key> inline size_t linked_unordered_set<Key>::size() const  { return m_lookup_set.size(); }

template <typename Key>
inline void
linked_unordered_set<Key>::push_back(const Key& key) {
  if (m_lookup_set.contains(key))
    return;

  m_order_list.push_back(key);
  m_lookup_set[key] = std::prev(m_order_list.end());
}

template <typename Key>
inline Key
linked_unordered_set<Key>::pop_front() {
  auto front_key = std::move(m_order_list.front());

  m_order_list.pop_front();
  m_lookup_set.erase(front_key);

  try_rebalance_after_erase();

  return front_key;
}

template <typename Key>
inline bool
linked_unordered_set<Key>::erase(const Key& key) {
  auto set_itr = m_lookup_set.find(key);

  if (set_itr == m_lookup_set.end())
    return false;

  m_order_list.erase(set_itr->second);
  m_lookup_set.erase(set_itr);

  try_rebalance_after_erase();

  return true;
}

// Rebalance the underlying set if we have been under a certain threshold for a number of operations.
template <typename Key>
inline void
linked_unordered_set<Key>::try_rebalance_after_erase() {
  // Bucket sizes are primes, but 32 is a good threshold to start rebalancing at.
  if (m_lookup_set.bucket_count() < 32)
    return;

  if (m_lookup_set.size() == 0) {
    m_lookup_set.clear();
    m_under_threshold_count = 0;
    return;
  }

  if (m_lookup_set.size() > m_lookup_set.bucket_count() / 4) {
    m_under_threshold_count = 0;
    return;
  }

  m_under_threshold_count++;

  if (m_under_threshold_count < m_lookup_set.bucket_count() / 6)
    return;

  m_lookup_set.reserve(m_lookup_set.size() * 2);
}

} // namespace torrent::utils

#endif
