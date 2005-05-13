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

#include "torrent/exceptions.h"
#include "path.h"

namespace torrent {

void
Path::insert_path(iterator pos, const std::string& path) {
  std::string::const_iterator first = path.begin();
  std::string::const_iterator last;

  while (first != path.end()) {
    pos = insert(pos, std::string(first, (last = std::find(first, path.end(), '/'))));

    if (last == path.end())
      return;

    first = last;
    first++;
  }
}

std::string
Path::as_string() {
  if (empty())
    return std::string();

  std::string s;

  for (iterator itr = begin(); itr != end(); ++itr) {
    s += '/';
    s += *itr;
  }

  return s;
}

void
Path::mkdir(const std::string& root,
	    Base::const_iterator pathBegin, Base::const_iterator pathEnd,
	    Base::const_iterator ignoreBegin, Base::const_iterator ignoreEnd,
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
