#include "config.h"

#include <algo/algo.h>

#include "path.h"

using namespace algo;

namespace torrent {

std::string
Path::path(bool escaped) {
  std::string p;

  if (escaped)
    std::for_each(m_list.begin(), m_list.end(),
		  add_ref(p, add(call(&Path::escape, back_as_ref()), value("/"))));
  else
    std::for_each(m_list.begin(), m_list.end(),
		  add_ref(p, add(back_as_ref(), value("/"))));

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

}

