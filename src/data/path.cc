// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <errno.h>
#include <sys/stat.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"
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
Path::mkdir(const std::string& root,
	    List::const_iterator pathBegin, List::const_iterator pathEnd,
	    List::const_iterator ignoreBegin, List::const_iterator ignoreEnd,
	    unsigned int umask) {

  std::string p = root;

  while (pathBegin != pathEnd) {
    p += "/" + *pathBegin;

    if (ignoreBegin == ignoreEnd ||
	*pathBegin != *ignoreBegin) {

      ignoreBegin = ignoreEnd;

      if (::mkdir(p.c_str(), umask) &&
	  errno != EEXIST)
	throw storage_error("Could not create directory '" + p + "': " + strerror(errno));

    } else {
      ++ignoreBegin;
    }

    ++pathBegin;
  }
}

void
Path::mkdir(const std::string& dir, unsigned int umask) {
  if (::mkdir(dir.c_str(), umask) &&
      errno != EEXIST)
    throw storage_error("Could not create directory '" + dir + "': " + strerror(errno));
}

}
