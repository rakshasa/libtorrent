#ifndef LIBTORRENT_RANGES_H
#define LIBTORRENT_RANGES_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class Ranges {
public:
  typedef std::pair<uint32_t, uint32_t> Range;
  typedef std::pair<uint32_t, Range>    Position;
  typedef std::vector<Range>            List;

  Ranges()                                { clear(); }

  // Must be added in increasing order.
  void                add(uint32_t begin, uint32_t end);

  void                clear()             { m_size = 0; m_list.clear(); }


  unsigned int        get_size()          { return m_size; }
  List&               get_list()          { return m_list; }

  List::iterator      find(uint32_t index);

  bool                has(uint32_t index);

private:
  uint32_t  m_size;
  List      m_list;
};

}

#endif
  
