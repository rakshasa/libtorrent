#ifndef LIBTORRENT_RANGES_H
#define LIBTORRENT_RANGES_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class Ranges : private std::vector<std::pair<uint32_t, uint32_t> > {
public:
  typedef std::pair<uint32_t, uint32_t>        Range;
  typedef std::vector<Range>                   List;

  using List::iterator;
  using List::reverse_iterator;
  using List::clear;
  using List::size;

  using List::begin;
  using List::end;
  using List::rbegin;
  using List::rend;

  void                insert(uint32_t begin, uint32_t end);
  void                erase(uint32_t begin, uint32_t end);

  iterator            find(uint32_t index);

  // Use find with no closest match.
  bool                has(uint32_t index);

private:
  List      m_list;
};

}

#endif
  
