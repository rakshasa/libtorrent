#ifndef LIBTORRENT_PRIORITY_H
#define LIBTORRENT_PRIORITY_H

#include <vector>

namespace torrent {

class Priority {
public:
  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Type;

  typedef std::pair<unsigned int, unsigned int> Range;
  typedef std::vector<Range>                    List;
  typedef std::pair<unsigned int, Range>        Position;

  Priority() { clear(); }

  // Must be added in increasing order.
  void                 add(Type t, unsigned int begin, unsigned int end);

  void                 clear();

  unsigned int         get_size(Type t) { return m_size[t]; }
  const List&          get_list(Type t) { return m_list[t]; }

  List::const_iterator find(Type t, unsigned int index);

private:
  unsigned int m_size[3];
  List         m_list[3];
};

}

#endif
  
