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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DATA_FILE_META_H
#define LIBTORRENT_DATA_FILE_META_H

#include <rak/functional.h>

#include "file.h"
#include "globals.h"

namespace torrent {

class FileManager;

class FileMeta {
public:
  typedef rak::mem_fun3<FileManager, bool, FileMeta*, int, int> SlotPrepare;

  FileMeta() : m_lastTouched(cachedTime) {}

  bool                is_open() const                            { return m_file.is_open(); }
  bool                has_permissions(int prot) const            { return !(prot & ~get_prot()); }

  // Consider prot == 0 to close?
  inline bool         prepare(int prot, int flags = 0);

  File&               get_file()                                 { return m_file; }
  const File&         get_file() const                           { return m_file; }

  const std::string&  get_path() const                           { return m_path; }
  void                set_path(const std::string& path)          { m_path = path; }

  int                 get_prot() const                           { return m_file.get_prot(); }

  rak::timer          get_last_touched() const                   { return m_lastTouched; }
  void                set_last_touched(rak::timer t = cachedTime) { m_lastTouched = t; }

  void                slot_prepare(SlotPrepare s)                { m_slotPrepare = s; }

private:
  File                m_file;
  std::string         m_path;

  rak::timer          m_lastTouched;
  SlotPrepare         m_slotPrepare;
};

inline bool
FileMeta::prepare(int prot, int flags) {
  m_lastTouched = cachedTime;

  return (is_open() && has_permissions(prot)) || m_slotPrepare(this, prot, flags);
}

}

#endif
