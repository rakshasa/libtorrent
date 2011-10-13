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
  m_tracker_list(new TrackerList()),

  m_numRequests(0),
  m_maxRequests(4),
  m_initialTracker(0) {

  m_tracker_controller = new TrackerController(m_tracker_list);
  m_tracker_controller->slot_success() = std::bind(&TrackerManager::receive_success, this, std::placeholders::_1);
  m_tracker_controller->slot_failure() = std::bind(&TrackerManager::receive_failed, this, std::placeholders::_1);

  m_tracker_list->slot_success() = std::bind(&TrackerController::receive_success, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);
  m_tracker_list->slot_failure() = std::bind(&TrackerController::receive_failure, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);

  m_tracker_controller->task_timeout()->set_slot(rak::mem_fn(this, &TrackerManager::receive_timeout));
}

TrackerManager::~TrackerManager() {
  if (m_tracker_controller->is_active())
    throw internal_error("TrackerManager::~TrackerManager() called but is_active() != false.");

  m_tracker_list->clear();
  delete m_tracker_list;
  delete m_tracker_controller;
}

void
TrackerManager::send_start() {
  tracker_controller()->close();

  m_tracker_list->set_focus(m_tracker_list->begin());
  m_tracker_list->send_state(DownloadInfo::STARTED);
}

void
TrackerManager::send_stop() {
  tracker_controller()->close();

  m_tracker_list->set_focus(m_tracker_list->begin() + m_initialTracker);
  m_tracker_list->send_state(DownloadInfo::STOPPED);
}

void
TrackerManager::send_completed() {
  tracker_controller()->close();
  m_tracker_list->send_state(DownloadInfo::COMPLETED);
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
  if (m_tracker_list->has_active() || m_numRequests >= m_maxRequests)
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
  m_numRequests  = 0;
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

void
TrackerManager::insert(int group, const std::string& url) {
  if (m_tracker_list->has_active())
    throw internal_error("Tried to add tracker while a tracker is active.");

  Tracker* t;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0)
    t = new TrackerHttp(m_tracker_list, url);

  else if (std::strncmp("udp://", url.c_str(), 6) == 0)
    t = new TrackerUdp(m_tracker_list, url);

  else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed())
    t = new TrackerDht(m_tracker_list, url);

  else
    // TODO: Error message here?... not really...
    return;
  
  m_tracker_list->insert(group, t);
}

void
TrackerManager::receive_timeout() {
  if (m_tracker_list->has_active())
    throw internal_error("TrackerManager::receive_timeout() called but m_tracker_list->has_active() == true.");

  if (!m_tracker_controller->is_active())
    return;

  m_tracker_list->send_state((DownloadInfo::State)m_tracker_list->state());
}

void
TrackerManager::receive_success(AddressList* l) {
  if (m_tracker_list->state() == DownloadInfo::STOPPED || !m_tracker_controller->is_active())
    return slot_success()(l);

  if (m_tracker_list->state() == DownloadInfo::STARTED)
    m_initialTracker = std::distance(m_tracker_list->begin(), m_tracker_list->focus());

  // Don't reset the focus when we're requesting more peers. If we
  // want to query the next tracker in the list we need to remember
  // the current focus.
  if (m_tracker_controller->is_requesting()) {
    m_numRequests++;
  } else {
    m_numRequests = 1;
    m_tracker_list->set_focus(m_tracker_list->begin());
  }

  // Reset m_isRequesting so a new call to request_*() is needed to
  // try from the rest of the trackers in the list. If not called, the
  // next tracker request will reset the focus to the first tracker.
  m_tracker_controller->stop_requesting();

  m_tracker_list->set_state(DownloadInfo::NONE);
  priority_queue_erase(&taskScheduler, m_tracker_controller->task_timeout());
  priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), (cachedTime + rak::timer::from_seconds(m_tracker_list->focus_normal_interval())).round_seconds());

  slot_success()(l);
}

void
TrackerManager::receive_failed(const std::string& msg) {
  if (m_tracker_list->focus() != m_tracker_list->end())
    m_tracker_list->set_focus(m_tracker_list->focus() + 1);

  if (m_tracker_list->state() == DownloadInfo::STOPPED || !m_tracker_controller->is_active())
    return slot_failure()(msg);

  priority_queue_erase(&taskScheduler, m_tracker_controller->task_timeout());

  if (m_tracker_controller->is_requesting()) {
    // Currently trying to request additional peers.

    if (m_tracker_list->focus() == m_tracker_list->end()) {
      // Don't start from the beginning of the list if we've gone
      // through the whole list. Return to normal timeout.
      m_tracker_controller->stop_requesting();
      priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), (cachedTime + rak::timer::from_seconds(m_tracker_list->focus_normal_interval())).round_seconds());
    } else {
      priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), (cachedTime + rak::timer::from_seconds(20)).round_seconds());
    }

  } else {
    // Normal retry.
    unsigned int retry_seconds = 3;

    if (m_tracker_list->focus() == m_tracker_list->end()) {
      // Tried all the trackers, start from the beginning.
      m_tracker_controller->set_failed_requests(m_tracker_controller->failed_requests() + 1);
      m_tracker_list->set_focus(m_tracker_list->begin());

      retry_seconds = std::min<unsigned int>(300, 3 + 20 * m_tracker_controller->failed_requests());
    }

    priority_queue_insert(&taskScheduler, m_tracker_controller->task_timeout(), (cachedTime + rak::timer::from_seconds(retry_seconds)).round_seconds());
  }

  slot_failure()(msg);
}

}
