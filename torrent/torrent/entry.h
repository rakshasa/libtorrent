#ifndef LIBTORRENT_ENTRY_H
#define LIBTORRENT_ENTRY_H

#include <torrent/common.h>

namespace torrent {

class Entry {
public:
  typedef std::pair<uint32_t, uint32_t> Range;

  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Priority;

  Entry()        : m_entry(NULL) {}
  Entry(void* e) : m_entry(e) {}
  
  uint64_t         get_size();
  Range            get_range();
  uint32_t         get_completed();

  // Relative to root of torrent.
  std::string      get_path();

  Priority         get_priority();

  void             set_priority(Priority p);

private:
  void*            m_entry;
};

}

#endif
