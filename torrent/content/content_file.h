#ifndef LIBTORRENT_CONTENT_FILE_H
#define LIBTORRENT_CONTENT_FILE_H

#include <vector>
#include <string>

namespace torrent {

class ContentFile {
public:
  typedef std::vector<std::string> FileName;

  ContentFile(const Filename& f, uint64_t size) : m_filename(f), m_size(size) {}

  uint64_t  size() { return m_size; }

  FileName& filename() { return m_filename; }

private:
  FileName m_filename;

  uint64_t m_size;
};

}

#endif

