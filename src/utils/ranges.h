#ifndef LIBTORRENT_RANGES_H
#define LIBTORRENT_RANGES_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class Ranges : private std::vector<std::pair<uint32_t, uint32_t> > {
public:
  typedef std::vector<std::pair<uint32_t, uint32_t> > Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::clear;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  void                insert(uint32_t first, uint32_t last) { insert(std::make_pair(first, last)); }
  void                erase(uint32_t first, uint32_t last)  { erase(std::make_pair(first, last)); }

  void                insert(value_type r);
  void                erase(value_type r);

  // Find the first ranges that has an end greater than index.
  iterator            find(uint32_t index);

  // Use find with no closest match.
  bool                has(uint32_t index);

  // Remove ranges in r from this.
  Ranges&             intersect(Ranges& r);
};

}

#endif
