#ifndef LIBTORRENT_PRIORITY_H
#define LIBTORRENT_PRIORITY_H

#include "utils/ranges.h"

namespace torrent {

class Priority {
public:
  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Type;

  typedef Ranges::List  List;
  typedef Ranges::Range Range;

    // Must be added in increasing order.
  void                add(Type t, uint32_t begin, uint32_t end) { m_ranges[t].add(begin, end); }

  void                clear()                                   { for (int i = 0; i < 3; ++i) m_ranges[i].clear(); }

  unsigned int        get_size(Type t)                          { return m_ranges[t].get_size(); }
  List&               get_list(Type t)                          { return m_ranges[t].get_list(); }

  List::iterator      find(Type t, uint32_t index)              { return m_ranges[t].find(index); }

  bool                has(Type t, uint32_t index)               { return m_ranges[t].has(index); }

private:
  Ranges    m_ranges[3];
};

}

#endif
