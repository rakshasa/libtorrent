#ifndef LIBTORRENT_BLOCK_FAILED_H
#define LIBTORRENT_BLOCK_FAILED_H

#include <algorithm>
#include <functional>
#include <vector>
#include <torrent/common.h>

namespace torrent {

class BlockFailed : public std::vector<std::pair<char*, uint32_t> > {
public:
  using base_type = std::vector<std::pair<char*, uint32_t>>;

  using base_type::value_type;
  using base_type::reference;
  using base_type::size_type;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::operator[];

  static constexpr uint32_t invalid_index = ~uint32_t();

  BlockFailed() = default;
  ~BlockFailed();
  BlockFailed(const BlockFailed&) = delete;
  BlockFailed& operator=(const BlockFailed&) = delete;

  size_type           current() const                   { return m_current; }
  iterator            current_iterator()                { return begin() + m_current; }
  reverse_iterator    current_reverse_iterator()        { return reverse_iterator(begin() + m_current + 1); }

  void                set_current(size_type idx)        { m_current = idx; }
  void                set_current(iterator itr)         { m_current = itr - begin(); }
  void                set_current(reverse_iterator itr) { m_current = itr.base() - begin() - 1; }

  iterator            max_element();
  reverse_iterator    reverse_max_element();

private:
  static void         delete_entry(value_type e)                    { delete [] e.first; }
  static bool         compare_entries(value_type e1, value_type e2) { return e1.second < e2.second; }

  size_type           m_current{invalid_index};
};

inline
BlockFailed::~BlockFailed() {
  std::for_each(begin(), end(), &BlockFailed::delete_entry);
}

inline BlockFailed::iterator
BlockFailed::max_element() {
  return std::max_element(begin(), end(), &BlockFailed::compare_entries);
}

inline BlockFailed::reverse_iterator
BlockFailed::reverse_max_element() {
  return std::max_element(rbegin(), rend(), &BlockFailed::compare_entries);
}

} // namespace torrent

#endif
