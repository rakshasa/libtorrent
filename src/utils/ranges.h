#ifndef LIBTORRENT_RANGES_H
#define LIBTORRENT_RANGES_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class Ranges {
public:
  typedef std::pair<uint32_t, uint32_t>        Range;
  typedef std::vector<Range>                   List;

  typedef std::vector<Range>::iterator         iterator;
  typedef std::vector<Range>::reverse_iterator reverse_iterator;

  void                clear()             { m_list.clear(); }

  List::size_type     size()              { return m_list.size(); }

  void                insert(uint32_t begin, uint32_t end);
  void                erase(uint32_t begin, uint32_t end);

  iterator            begin()             { return m_list.begin(); }
  iterator            end()               { return m_list.end(); }

  reverse_iterator    rbegin()            { return m_list.rbegin(); }
  reverse_iterator    rend()              { return m_list.rend(); }

  iterator            find(uint32_t index);

  // Use find with no closest match.
  bool                has(uint32_t index);

private:
  List      m_list;
};

}

#endif
  
