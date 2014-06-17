// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#ifndef LIBTORRENT_DIRECTORY_EVENTS_H
#define LIBTORRENT_DIRECTORY_EVENTS_H

#include <string>
#include <vector>
#include lt_tr1_functional
#include <torrent/event.h>

namespace torrent {

struct watch_descriptor {
  typedef std::function<void (const std::string&)> slot_string;

  bool compare_desc(int desc) const { return desc == descriptor; }

  int         descriptor;
  std::string path;
  slot_string slot;
};

class LIBTORRENT_EXPORT directory_events : public Event {
public:
  typedef std::vector<watch_descriptor> wd_list;
  typedef watch_descriptor::slot_string slot_string;

  static const int flag_on_added   = 0x1;
  static const int flag_on_removed = 0x2;
  static const int flag_on_updated = 0x3;

  directory_events() { m_fileDesc = -1; }
  ~directory_events() {}

  bool                open();
  void                close();

  void                notify_on(const std::string& path, int flags, const slot_string& slot);

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

  virtual const char* type_name() const { return "directory_events"; }

private:
  wd_list             m_wd_list;
};

}

#endif
