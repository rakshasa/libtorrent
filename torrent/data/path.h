#ifndef LIBTORRENT_PATH_H
#define LIBTORRENT_PATH_H

#include <string>
#include <list>

namespace torrent {

// Tip of the century, use a blank first path to get root and "." to get current dir.

class Path {
public:
  typedef std::list<std::string> List;

  Path(const List& l) : m_list(l) {}

  List&              list() { return m_list; }

  std::string        path(bool escaped = true);

  static std::string escape(const std::string& s);

private:
  List m_list;
};

}

#endif

