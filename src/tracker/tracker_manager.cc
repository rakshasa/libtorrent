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

#include "config.h"

#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"

#include "tracker_dht.h"
#include "tracker_http.h"
#include "tracker_manager.h"
#include "tracker_udp.h"

namespace std { using namespace tr1; }

namespace torrent {

TrackerManager::TrackerManager() :
  m_tracker_list(new TrackerList()) {

  m_tracker_controller = new TrackerController(m_tracker_list);

  m_tracker_list->slot_success() = std::bind(&TrackerController::receive_success_new, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);
  m_tracker_list->slot_failure() = std::bind(&TrackerController::receive_failure_new, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);
}

TrackerManager::~TrackerManager() {
  if (m_tracker_controller->is_active())
    throw internal_error("TrackerManager::~TrackerManager() called but is_active() != false.");

  m_tracker_list->clear();
  delete m_tracker_list;
  delete m_tracker_controller;
}

void
TrackerManager::send_later() {
  if (m_tracker_list->has_active())
    return;

  if (m_tracker_list->state() == DownloadInfo::STOPPED)
    throw internal_error("TrackerManager::send_later() m_tracker_list->set() == DownloadInfo::STOPPED.");

  rak::timer t(std::max(cachedTime + rak::timer::from_seconds(2),
                        rak::timer::from_seconds(m_tracker_list->time_last_connection() + m_tracker_list->focus_min_interval())));

  priority_queue_erase(&taskScheduler, m_tracker_controller->task_timeout());
  priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), t);
}

// When request_{current,next} is called, m_isRequesting is set to
// true. This ensures that if none of the remaining trackers can be
// reached or if a connection is successfull, it will not reset the
// focus to the first tracker.
//
// The client can therefor call these functions after
// TrackerList::signal_success is emited and know it won't cause
// looping if there are unreachable trackers.
//
// When the number of consequtive requests from the same tracker
// through this function has reached a certain limit, it will stop the
// request. 'm_maxRequests' thus makes sure that a client with a very
// high "min peers" setting will not cause too much traffic.
bool
TrackerManager::request_current() {
  if (m_tracker_list->has_active())// || m_numRequests >= m_maxRequests)
    return false;

  // Keep track of how many times we've requested from the current
  // tracker without waiting for some minimum interval.
  m_tracker_controller->start_requesting();
  manual_request(true);

  return true;
}

void
TrackerManager::request_next() {
  // Check next against last successfull connection?
  if (m_tracker_list->has_active() || !m_tracker_list->focus_next_group())
    return;

  m_tracker_controller->start_requesting();
  //  m_numRequests  = 0;
  manual_request(true);
}

// Manual requests do not change the status of m_isRequesting, so if
// it is trying to retrive more peers only the current timeout will be
// affected.
void
TrackerManager::manual_request(bool force) {
  if (!m_tracker_controller->task_timeout()->is_queued())
    return;

  rak::timer t(cachedTime + rak::timer::from_seconds(2));
  
  if (!force)
    t = std::max(t, rak::timer::from_seconds(m_tracker_list->time_last_connection() + m_tracker_list->focus_min_interval()));

  priority_queue_erase(&taskScheduler, m_tracker_controller->task_timeout());
  priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), t.round_seconds());
}

}
