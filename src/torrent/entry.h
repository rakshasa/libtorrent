#ifndef LIBTORRENT_ENTRY_H
#define LIBTORRENT_ENTRY_H

#include <list>
#include <torrent/common.h>

namespace torrent {

class Entry {
public:
  typedef std::pair<uint32_t, uint32_t> Range;
  typedef std::list<std::string>        Path;

  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Priority;

  Entry() : m_entry(NULL) {}
  Entry(void* e) : m_entry(e) {}
  
  uint64_t        get_size();

  // Chunks of this file completed.
  uint32_t        get_completed();

  // Chunk index this file spans.
  uint32_t        get_chunk_begin();
  uint32_t        get_chunk_end();

  // Need this?
  //uint64_t        get_byte_begin();
  //uint64_t        get_byte_end();

  // Relative to root of the torrent.
  std::string     get_path();
  const Path&     get_path_list();

  void            set_path_list(const Path& l);

  Priority        get_priority();

  // Remember to call update_priorities
  void            set_priority(Priority p);

private:
  void*           m_entry;
};

}

#endif
