#include "config.h"

#include <errno.h>
#include <sys/stat.h>
#include <algo/algo.h>

#include "exceptions.h"
#include "path.h"

using namespace algo;

namespace torrent {

Path::Path(const std::string& p, bool literal) {
  if (literal) {
    m_list.push_back(p);

  } else {

    std::string::size_type cur, last = 0;

    do {
      m_list.push_back(p.substr(last, (cur = p.find('/', last))));

    } while ((last = cur) != std::string::npos);
  }
}

std::string
Path::path(bool escaped) {
  std::string p;

  if (m_list.empty())
    return p;

  if (escaped)
    std::for_each(m_list.begin(), m_list.end(),
		  add_ref(p, add(value("/"), call(&Path::escape, back_as_ref()))));
  else
    std::for_each(m_list.begin(), m_list.end(),
		  add_ref(p, add(value("/"), back_as_ref())));

  return p;
}

std::string
Path::escape(const std::string& s) {
  std::string d;

  for (std::string::const_iterator itr = s.begin(); itr != s.end(); ++itr) {
    if (*itr == '/' ||
	*itr == '\\')
      d.push_back('\\');

    d.push_back(*itr);
  }

  return d;
}

void
Path::mkdir(const std::string& root, const Path& path, const Path& ignore, int umask) {
  std::string p = root;

  for (List::const_iterator itr = path.m_list.begin(), jtr = ignore.m_list.begin(); itr != path.m_list.end(); ++itr) {
    p += *itr;

    if (jtr == ignore.m_list.end() || *itr != *jtr) {
      jtr = ignore.m_list.end();

      if (::mkdir(p.c_str(), umask) &&
	  errno != EEXIST)
	throw storage_error("Could not create directory '" + p + "': " + strerror(errno));

    } else {
      ++jtr;
    }
  }
}

}
