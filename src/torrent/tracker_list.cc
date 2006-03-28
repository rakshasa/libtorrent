// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "config.h"

#include "download/download_info.h"
#include "tracker/tracker_manager.h"

#include "exceptions.h"
#include "tracker.h"
#include "tracker_list.h"

namespace torrent {

Tracker
TrackerList::get(uint32_t index) {
  if (index >= m_manager->size())
    throw client_error("Client called TrackerList::get_tracker(...) with out of range index.");

  return m_manager->get(index);
}

const Tracker
TrackerList::get(uint32_t index) const {
  if (index >= m_manager->size())
    throw client_error("Client called TrackerList::get_tracker(...) with out of range index.");

  return m_manager->get(index);
}

uint32_t
TrackerList::size() const {
  return m_manager->size();
}

uint32_t
TrackerList::focus() const {
  return m_manager->focus_index();
}

uint64_t
TrackerList::timeout() const {
  return std::max(m_manager->get_next_timeout() - cachedTime, rak::timer()).usec();
}

int16_t
TrackerList::numwant() const {
  return m_manager->info()->numwant();
}

void
TrackerList::set_numwant(int32_t n) {
  m_manager->info()->set_numwant(n);
}

void
TrackerList::send_completed() {
  m_manager->send_completed();
}

void
TrackerList::cycle_group(int group) {
  m_manager->cycle_group(group);
}

void
TrackerList::manual_request(bool force) {
  m_manager->manual_request(force);
}

}
