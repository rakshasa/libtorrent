#ifndef LIBTORRENT_ENTRY_H
#define LIBTORRENT_ENTRY_H

#include <string>
#include <inttypes.h>

namespace torrent {

class ContentFile;

class Entry {
public:
  typedef std::pair<unsigned int, unsigned int> Range;

  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Priority;

  Entry() : m_entry(NULL) {}
  Entry(ContentFile* e) : m_entry(e) {}
  
  uint64_t          get_size();
  Range             get_range();
  unsigned int      get_completed();

  Priority          get_priority();

  // Relative to root of torrent.
  const std::string get_path();

  void              set_priority(Priority p);

private:
  ContentFile* m_entry;
};

}

#endif
