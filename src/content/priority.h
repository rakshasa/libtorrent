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

  typedef Ranges::iterator         iterator;
  typedef Ranges::reverse_iterator reverse_iterator;

  // Must be added in increasing order.
  void                add(Type t, uint32_t begin, uint32_t end) { m_ranges[t].insert(begin, end); }

  void                clear()                                   { for (int i = 0; i < 3; ++i) m_ranges[i].clear(); }

  iterator            begin(Type t)                             { return m_ranges[t].begin(); }
  iterator            end(Type t)                               { return m_ranges[t].end(); }

  reverse_iterator    rbegin(Type t)                            { return m_ranges[t].rbegin(); }
  reverse_iterator    rend(Type t)                              { return m_ranges[t].rend(); }

  iterator            find(Type t, uint32_t index)              { return m_ranges[t].find(index); }

  bool                has(Type t, uint32_t index)               { return m_ranges[t].has(index); }

private:
  Ranges              m_ranges[3];
};

}

#endif
