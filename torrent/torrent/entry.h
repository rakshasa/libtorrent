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

  Entry() : m_entry(NULL) {}
  Entry(void* e) : m_entry(e) {}
  
  uint64_t        get_size();
  uint32_t        get_completed();

  uint32_t        get_chunk_begin();
  uint32_t        get_chunk_end();

  // Need this?
  //uint64_t        get_byte_begin();
  //uint64_t        get_byte_end();

  // Relative to root of torrent.
  std::string     get_path();

  Priority        get_priority();

  // Remember to call update_priorities
  void            set_priority(Priority p);

private:
  void*           m_entry;
};

}

#endif
