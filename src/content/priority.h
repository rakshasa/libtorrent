#ifndef LIBTORRENT_PRIORITY_H
#define LIBTORRENT_PRIORITY_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class Priority {
public:
  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Type;

  typedef std::pair<uint32_t, uint32_t> Range;
  typedef std::vector<Range>            List;
  typedef std::pair<uint32_t, Range>    Position;

  Priority() { clear(); }

  // Must be added in increasing order.
  void                 add(Type t, uint32_t begin, uint32_t end);

  void                 clear();

  unsigned int         get_size(Type t) { return m_size[t]; }
  List&                get_list(Type t) { return m_list[t]; }

  List::iterator       find(Type t, uint32_t index);

  bool                 has(Type t, uint32_t index);

private:
  uint32_t m_size[3];
  List     m_list[3];
};

}

#endif
  
