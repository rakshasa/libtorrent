#ifndef LIBTORRENT_PRIORITY_H
#define LIBTORRENT_PRIORITY_H

namespace torrent {

class Priority {
public:
  typedef std::pair<unsigned int, unsigned int> Range;

  typedef enum {
    STOPPED,
    NORMAL,
    HIGH
  } Type;

  void  add(unsigned int begin, unsigned int end);

  void  set(unsigned int index, Type priority);

  Range get(unsigned int index, Type priority);

private:
};

}

#endif
  
