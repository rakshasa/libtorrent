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

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_STATUS_H
#define LIBTORRENT_DOWNLOAD_CHOKE_STATUS_H

#include <torrent/common.h>

namespace torrent {

class group_entry;

class choke_status {
public:
  choke_status() :
    m_group_entry(NULL),

    m_queued(false),
    m_unchoked(false),
    m_snubbed(false),
    m_timeLastChoke(0) {}

  group_entry*        entry() const                           { return m_group_entry; }
  void                set_entry(group_entry* grp_ent)         { m_group_entry = grp_ent; }

  bool                queued() const                          { return m_queued; }
  void                set_queued(bool s)                      { m_queued = s; }

  bool                choked() const                          { return !m_unchoked; }
  bool                unchoked() const                        { return m_unchoked; }
  void                set_unchoked(bool s)                    { m_unchoked = s; }

  bool                snubbed() const                         { return m_snubbed; }
  void                set_snubbed(bool s)                     { m_snubbed = s; }

  int64_t             time_last_choke() const                 { return m_timeLastChoke; }
  void                set_time_last_choke(int64_t t)          { m_timeLastChoke = t; }

private:
  // TODO: Use flags.
  group_entry*        m_group_entry;

  bool                m_queued;
  bool                m_unchoked;
  bool                m_snubbed;

  int64_t             m_timeLastChoke;
};

}

#endif
