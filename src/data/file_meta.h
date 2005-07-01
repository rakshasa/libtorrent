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

#include <sigc++/slot.h>

#include "utils/timer.h"
#include "file.h"

namespace torrent {

class FileMeta {
public:
  typedef sigc::slot2<bool, FileMeta*, int> SlotPrepare;
  typedef sigc::slot1<void, FileMeta*>      SlotDisconnect;

  FileMeta() {}
  FileMeta(const std::string& path) : m_path(path) {}
  ~FileMeta() { disconnect(); }

  bool                is_valid() const                           { return !m_slotPrepare.empty(); }
  bool                is_open() const                            { return m_file.is_open(); }

  bool                has_permissions(int prot) const            { return !(prot & ~get_prot()); }

  inline bool         prepare(int prot);
  inline void         disconnect();

  File&               get_file()                                 { return m_file; }
  const File&         get_file() const                           { return m_file; }

  const std::string&  get_path() const                           { return m_path; }
  void                set_path(const std::string& path)          { m_path = path; }

  int                 get_prot() const                           { return m_file.get_prot(); }

  Timer               get_last_touched() const                   { return m_lastTouched; }
  void                set_last_touched(Timer t = Timer::cache()) { m_lastTouched = t; }

  SlotPrepare&        slot_prepare()                             { return m_slotPrepare; }
  SlotDisconnect&     slot_disconnect()                          { return m_slotDisconnect; }

private:
  File                m_file;
  std::string         m_path;

  Timer               m_lastTouched;

  SlotPrepare         m_slotPrepare;
  SlotDisconnect      m_slotDisconnect;
};

inline bool
FileMeta::prepare(int prot) {
  m_lastTouched = Timer::cache();

  return (is_open() && has_permissions(prot)) ? true : m_slotPrepare(this, prot);
}

inline void
FileMeta::disconnect() {
  m_slotDisconnect(this);
}

}

#endif
