#ifndef LIBTORRENT_CONTENT_FILE_H
#define LIBTORRENT_CONTENT_FILE_H

#include <vector>
#include <string>

#include "data/path.h"

namespace torrent {

class ContentFile {
public:
  ContentFile(const Path& p, uint64_t size) : m_path(p), m_size(size) {}

  uint64_t    size() const { return m_size; }

  Path&       path() { return m_path; }
  const Path& path() const { return m_path; }

private:
  Path     m_path;
  uint64_t m_size;
};

}

#endif

