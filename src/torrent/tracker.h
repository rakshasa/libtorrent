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

#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <torrent/common.h>

namespace torrent {

class TrackerHttp;

// Consider changing into a Download + tracker id.

class Tracker {
public:
  typedef std::pair<int, TrackerHttp*> value_type;

  Tracker()             : m_tracker(value_type(0, NULL)) {}
  Tracker(value_type v) : m_tracker(v) {}
  
  uint32_t            get_group()                        { return m_tracker.first; }
  const std::string&  get_url();

  // The "tracker id" string returned by the tracker.
  const std::string&  get_tracker_id();

private:
  value_type          m_tracker;
};

}

#endif
