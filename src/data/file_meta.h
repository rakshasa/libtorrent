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

#ifndef LIBTORRENT_DATA_FILE_META_H
#define LIBTORRENT_DATA_FILE_META_H

#include "utils/timer.h"
#include "file.h"

namespace torrent {

class FileMeta {
public:
  FileMeta(const std::string& path) : m_path(path) {}

  bool                is_open() const                            { return m_file.is_open(); }

  File&               get_file()                                 { return m_file; }
  const File&         get_file() const                           { return m_file; }

  const std::string&  get_path() const                           { return m_path; }

  Timer               get_last_touched() const                   { return m_lastTouched; }
  void                set_last_touched(Timer t = Timer::cache()) { m_lastTouched = t; }

  // Return true if *this is more active f and open.
  inline bool         comp_less_active(const FileMeta* const f) const;

private:
  File                m_file;
  std::string         m_path;

  Timer               m_lastTouched;
};

inline bool
FileMeta::comp_less_active(const FileMeta* const f) const {
  return (m_lastTouched < f->m_lastTouched && is_open()) || !f->is_open();
}

}

#endif
