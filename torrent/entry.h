#ifndef LIBTORRENT_ENTRY_H
#define LIBTORRENT_ENTRY_H

#include <string>
#include <inttypes.h>

namespace torrent {

class ContentFile;

class Entry {
public:
  Entry() : m_entry(NULL) {}
  Entry(ContentFile* e) : m_entry(e) {}
  
  uint64_t          size();

  // Relative to root of torrent.
  const std::string path();

private:
  ContentFile* m_entry;
};

}

#endif
