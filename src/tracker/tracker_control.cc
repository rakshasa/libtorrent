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

#include "config.h"

#include <algorithm>
#include <cstring>
#include <functional>

#include "net/address_list.h"
#include "torrent/exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"
#include "tracker_udp.h"

namespace torrent {

// m_tries is -1 if last connection wasn't successfull or we haven't tried yet.

TrackerControl::TrackerControl() :
  m_tries(-1),
  m_info(NULL),
  m_state(DownloadInfo::STOPPED),
  m_timeLastConnection(cachedTime) {
  
  m_itr = m_list.end();
}

void
TrackerControl::insert(int group, const std::string& url) {
  if (is_busy())
    throw internal_error("Added tracker url while the current tracker is busy");

  Tracker* t;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0)
    t = new TrackerHttp(this, url);

  else if (std::strncmp("udp://", url.c_str(), 6) == 0)
    t = new TrackerUdp(this, url);

  else
    // TODO: Error message here?... not really...
    return;
  
  m_list.insert(group, t);
  m_itr = m_list.begin();
}

void
TrackerControl::cycle_group(int group) {
  Tracker* tb = (m_itr != m_list.end()) ? *m_itr : NULL;

  m_list.cycle_group(group);
  m_itr = m_list.find(tb);
}

void
TrackerControl::send_state(DownloadInfo::State s) {
  // Reset the target tracker since we're doing a new request.
  if (m_itr != m_list.end())
    (*m_itr)->close();

  m_tries = -1;
  m_state = s;

  m_itr = m_list.find_enabled(m_itr);

  if (m_itr != m_list.end())
    (*m_itr)->send_state(m_state,
                              std::max<int64_t>(m_info->slot_completed()() - m_info->completed_baseline(), 0),
                              std::max<int64_t>(m_info->up_rate()->total() - m_info->uploaded_baseline(), 0),
                              m_info->slot_left()());
  else
    m_slotFailed("Tried all trackers.");
}

void
TrackerControl::set_focus_index(uint32_t v) {
  if (v > m_list.size())
    throw internal_error("TrackerControl::set_focus_index(...) received an out-of-bounds value.");

  // Don't allow change of focus while busy for the moment.
  if (m_itr != m_list.end() && (*m_itr)->is_busy())
    throw internal_error("TrackerControl::set_focus_index(...) called but m_itr is busy.");

  m_itr = m_list.begin() + v;
}

bool
TrackerControl::focus_next_group() {
  if (m_itr == m_list.end())
    return false;

  // Don't allow change of focus while busy for the moment.
  if ((*m_itr)->is_busy())
    throw internal_error("TrackerControl::focus_next_group(...) called but m_itr is busy.");

  m_itr = m_list.end_group((*m_itr)->group());
  return m_itr != m_list.end();
}

void
TrackerControl::receive_success(Tracker* tb, AddressList* l) {
//   if ((*m_itr)->get_data() != NULL)
//     m_signalDump.emit((*m_itr)->get_data());

  TrackerContainer::iterator itr = m_list.find(tb);

  if (itr != m_itr || m_itr == m_list.end() || (*m_itr)->is_busy())
    throw internal_error("TrackerControl::receive_success(...) called but the iterator is invalid.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  m_itr = m_list.promote(m_itr);

  l->sort();
  l->erase(std::unique(l->begin(), l->end()), l->end());

  m_timeLastConnection = cachedTime;
  m_slotSuccess(l);
}

void
TrackerControl::receive_failed(Tracker* tb, const std::string& msg) {
//   if ((*m_itr)->get_data() != NULL)
//     m_signalDump.emit((*m_itr)->get_data());

  TrackerContainer::iterator itr = m_list.find(tb);

  if (itr != m_itr || m_itr == m_list.end() || (*m_itr)->is_busy())
    throw internal_error("TrackerControl::receive_failed(...) called but the iterator is invalid.");

  m_itr++;
  m_slotFailed(msg);
}

uint32_t
TrackerControl::focus_normal_interval() const {
  if (m_itr == m_list.end()) {
    TrackerContainer::const_iterator itr = m_list.find_enabled(m_list.begin());
    
    if (itr == m_list.end())
      return 1800;

    return (*itr)->normal_interval();
  }

  return (*m_itr)->normal_interval();
}

uint32_t
TrackerControl::focus_min_interval() const {
  return 0;
}

}
