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

#ifndef LIBTORRENT_CONTENT_FILE_H
#define LIBTORRENT_CONTENT_FILE_H

#include <vector>
#include <string>

#include "data/path.h"

namespace torrent {

class ContentFile {
public:
  typedef std::pair<uint32_t, uint32_t> Range;

  ContentFile(const Path& p, off_t s, Range r) :
    m_path(p),
    m_size(s),
    m_range(r),
    m_completed(0),
    m_priority(1) {}

  off_t          get_size() const               { return m_size; }
  unsigned char  get_priority() const           { return m_priority; }

  Range          get_range() const              { return m_range; }
  uint32_t       get_completed() const          { return m_completed; }

  Path&          get_path()                     { return m_path; }
  const Path&    get_path() const               { return m_path; }

  void           set_priority(unsigned char t)  { m_priority = t; }
  void           set_completed(uint32_t v)      { m_completed = v; }

private:
  Path           m_path;
  off_t          m_size;
  Range          m_range;

  uint32_t       m_completed;
  unsigned char  m_priority;
};

}

#endif

