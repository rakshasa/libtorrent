#ifndef LIBTORRENT_CONTENT_FILE_H
#define LIBTORRENT_CONTENT_FILE_H

#include <vector>
#include <string>

#include "data/path.h"

namespace torrent {

class ContentFile {
public:
  typedef std::pair<unsigned int, unsigned int> Range;

  ContentFile(const Path& p, uint64_t s, Range r) :
    m_path(p),
    m_size(s),
    m_range(r),
    m_completed(0),
    m_priority(1) {}

  uint64_t       get_size() const               { return m_size; }
  unsigned char  get_priority() const           { return m_priority; }

  Range          get_range() const              { return m_range; }
  unsigned int   get_completed() const          { return m_completed; }

  Path&          get_path()                     { return m_path; }
  const Path&    get_path() const               { return m_path; }

  void           set_priority(unsigned char t)  { m_priority = t; }
  void           set_completed(unsigned int v)  { m_completed = v; }

private:
  Path           m_path;
  uint64_t       m_size;
  Range          m_range;

  unsigned int   m_completed;
  unsigned char  m_priority;
};

}

#endif

