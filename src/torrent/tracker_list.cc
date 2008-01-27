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

#include <rak/functional.h>

#include "download/download_info.h"
#include "net/address_list.h"
#include "tracker/tracker_manager.h"

#include "globals.h"
#include "exceptions.h"
#include "tracker.h"
#include "tracker_list.h"

namespace torrent {

TrackerList::TrackerList(TrackerManager* manager) :
  m_manager(manager),
  m_info(NULL),
  m_state(DownloadInfo::STOPPED),

  m_key(0),
  m_numwant(-1),
  m_timeLastConnection(0),

  m_itr(begin()) {
}

bool
TrackerList::has_active() const {
  return m_itr != end() && (*m_itr)->is_busy();
}

// Need a custom predicate because the is_usable function is virtual.
struct tracker_usable_t : public std::unary_function<TrackerList::value_type, bool> {
  bool operator () (const TrackerList::value_type& value) const { return value->is_usable(); }
};

bool
TrackerList::has_usable() const {
  return std::find_if(begin(), end(), tracker_usable_t()) != end();
}

void
TrackerList::close_all() {
  std::for_each(begin(), end(), std::mem_fun(&Tracker::close));
}

void
TrackerList::clear() {
  std::for_each(begin(), end(), rak::call_delete<Tracker>());
  base_type::clear();
}

void
TrackerList::send_state(int s) {
  // Reset the target tracker since we're doing a new request.
  if (m_itr != end())
    (*m_itr)->close();

  set_state(s);
  m_itr = find_usable(m_itr);

  if (m_itr != end())
    (*m_itr)->send_state(state());
  else
    m_manager->receive_failed("Tried all trackers.");
}

TrackerList::iterator
TrackerList::insert(unsigned int group, Tracker* t) {
  t->set_group(group);

  iterator itr = base_type::insert(end_group(group), t);

  m_itr = begin();
  return itr;
}

uint32_t
TrackerList::time_next_connection() const {
  return std::max(m_manager->get_next_timeout() - cachedTime, rak::timer()).seconds();
}

void
TrackerList::send_completed() {
  m_manager->send_completed();
}

void
TrackerList::manual_request(bool force) {
  m_manager->manual_request(force);
}

void
TrackerList::manual_cancel() {
  m_manager->close();
}

TrackerList::iterator
TrackerList::find_usable(iterator itr) {
  while (itr != end() && !tracker_usable_t()(*itr))
    ++itr;

  return itr;
}

TrackerList::const_iterator
TrackerList::find_usable(const_iterator itr) const {
  while (itr != end() && !tracker_usable_t()(*itr))
    ++itr;

  return itr;
}

TrackerList::iterator
TrackerList::begin_group(unsigned int group) {
  return std::find_if(begin(), end(), rak::less_equal(group, std::mem_fun(&Tracker::group)));
}

void
TrackerList::cycle_group(unsigned int group) {
  Tracker* trackerPtr = m_itr != end() ? *m_itr : NULL;

  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || (*itr)->group() != group)
    return;

  while (++itr != end() && (*itr)->group() == group) {
    std::iter_swap(itr, prev);
    prev = itr;
  }

  m_itr = find(trackerPtr);
}

TrackerList::iterator
TrackerList::promote(iterator itr) {
  iterator first = begin_group((*itr)->group());

  if (first == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group.");

  std::swap(first, itr);
  return first;
}

void
TrackerList::randomize_group_entries() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group((*itr)->group());
    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

bool
TrackerList::focus_next_group() {
  return (m_itr = end_group((*m_itr)->group())) != end();
}

uint32_t
TrackerList::focus_normal_interval() const {
  if (m_itr == end()) {
    const_iterator itr = find_usable(begin());
    
    if (itr == end())
      return 1800;

    return (*itr)->normal_interval();
  }

  return (*m_itr)->normal_interval();
}

uint32_t
TrackerList::focus_min_interval() const {
  return 0;
}

void
TrackerList::receive_success(Tracker* tb, AddressList* l) {
  iterator itr = find(tb);

  if (itr != m_itr || m_itr == end() || (*m_itr)->is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  m_itr = promote(m_itr);

  l->sort();
  l->erase(std::unique(l->begin(), l->end()), l->end());

  set_time_last_connection(cachedTime.seconds());
  m_manager->receive_success(l);
}

void
TrackerList::receive_failed(Tracker* tb, const std::string& msg) {
  iterator itr = find(tb);

  if (itr != m_itr || m_itr == end() || (*m_itr)->is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  m_itr++;
  m_manager->receive_failed(msg);
}

}
