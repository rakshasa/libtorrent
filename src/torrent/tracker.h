// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <string>
#include <torrent/common.h>

namespace torrent {

// Consider changing into a Download + tracker id.

class LIBTORRENT_EXPORT Tracker {
public:
  typedef TrackerBase* value_type;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP
  } Type;

  Tracker()             : m_tracker(NULL) {}
  Tracker(value_type v) : m_tracker(v) {}
  
  bool                is_enabled() const;
  bool                is_open() const;

  void                enable();
  void                disable();

  uint32_t            group() const;
  const std::string&  url() const;

  // The "tracker id" string returned by the tracker.
  const std::string&  tracker_id() const;
  Type                tracker_type() const;

  uint32_t            normal_interval() const;
  uint32_t            min_interval() const;

  uint64_t            scrape_time_last() const;
  uint32_t            scrape_complete() const;
  uint32_t            scrape_incomplete() const;
  uint32_t            scrape_downloaded() const;

private:
  value_type          m_tracker;
};

}

#endif
