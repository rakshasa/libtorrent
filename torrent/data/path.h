#ifndef LIBTORRENT_PATH_H
#define LIBTORRENT_PATH_H

#include <string>
#include <list>

namespace torrent {

// Tip of the century, use a blank first path to get root and "." to get current dir.

class Path {
public:
  typedef std::list<std::string> List;

  Path() {}
  Path(const std::string& p, bool literal = false);
  Path(const List& l) : m_list(l) {}

  List&              list() { return m_list; }

  std::string        path(bool escaped = true);

  static std::string escape(const std::string& s);

  static void        mkdir(const std::string& root,
			   List::const_iterator pathBegin, List::const_iterator pathEnd,
			   List::const_iterator ignoreBegin, List::const_iterator ignoreEnd,
			   int umask = 0777);
  
private:
  List m_list;
};

}

#endif

