#include "config.h"

#include "entry.h"
#include "content/content_file.h"

namespace torrent {

uint64_t
Entry::size() {
  return m_entry->size();
}

// Relative to root of torrent.
const std::string
Entry::path() {
  return m_entry->path().path(false);
}

}
