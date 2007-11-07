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

#include "download/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker.h"

#include "tracker_container.h"
#include "tracker_http.h"
#include "tracker_manager.h"
#include "tracker_udp.h"

namespace torrent {

TrackerManager::TrackerManager() :
  m_control(new TrackerContainer(this)),

  m_active(false),
  m_isRequesting(false),

  m_numRequests(0),
  m_maxRequests(3),
  m_failedRequests(0),
  m_initialTracker(0) {

  m_taskTimeout.set_slot(rak::mem_fn(this, &TrackerManager::receive_timeout));
}

TrackerManager::~TrackerManager() {
  if (is_active())
    throw internal_error("TrackerManager::~TrackerManager() called but is_active() != false.");

  m_control->clear();
  delete m_control;
}

bool
TrackerManager::is_busy() const {
  return m_control->has_active();
}

void
TrackerManager::close() {
  m_isRequesting = false;
  m_failedRequests = 0;

  m_control->close_all();
  priority_queue_erase(&taskScheduler, &m_taskTimeout);
}

void
TrackerManager::send_start() {
  close();

  m_control->set_focus(m_control->begin());
  m_control->send_state(DownloadInfo::STARTED);
}

void
TrackerManager::send_stop() {
  close();

  m_control->set_focus(m_control->begin() + m_initialTracker);
  m_control->send_state(DownloadInfo::STOPPED);
}

void
TrackerManager::send_completed() {
  close();
  m_control->send_state(DownloadInfo::COMPLETED);
}

void
TrackerManager::send_later() {
  if (m_control->has_active())
    return;

  if (m_control->state() == DownloadInfo::STOPPED)
    throw internal_error("TrackerManager::send_later() m_control->set() == DownloadInfo::STOPPED.");

  priority_queue_erase(&taskScheduler, &m_taskTimeout);
  priority_queue_insert(&taskScheduler, &m_taskTimeout, rak::timer::from_seconds(m_control->time_last_connection() + m_control->focus_min_interval()));
}

// When request_{current,next} is called, m_isRequesting is set to
// true. This ensures that if none of the remaining trackers can be
// reached or if a connection is successfull, it will not reset the
// focus to the first tracker.
//
// The client can therefor call these functions after
// TrackerContainer::signal_success is emited and know it won't cause
// looping if there are unreachable trackers.
//
// When the number of consequtive requests from the same tracker
// through this function has reached a certain limit, it will stop the
// request. 'm_maxRequests' thus makes sure that a client with a very
// high "min peers" setting will not cause too much traffic.
bool
TrackerManager::request_current() {
  if (m_control->has_active() || m_numRequests >= m_maxRequests)
    return false;

  // Keep track of how many times we've requested from the current
  // tracker without waiting for some minimum interval.
  m_isRequesting = true;
  manual_request(true);

  return true;
}

void
TrackerManager::request_next() {
  // Check next against last successfull connection?
  if (m_control->has_active() || !m_control->focus_next_group())
    return;

  m_isRequesting = true;
  m_numRequests  = 0;
  manual_request(true);
}

// Manual requests do not change the status of m_isRequesting, so if
// it is trying to retrive more peers only the current timeout will be
// affected.
void
TrackerManager::manual_request(bool force) {
  if (!m_taskTimeout.is_queued())
    return;

  rak::timer t(cachedTime + rak::timer::from_seconds(2));
  
  if (!force)
    t = std::max(t, rak::timer::from_seconds(m_control->time_last_connection() + m_control->focus_min_interval()));

  priority_queue_erase(&taskScheduler, &m_taskTimeout);
  priority_queue_insert(&taskScheduler, &m_taskTimeout, t.round_seconds());
}

void
TrackerManager::cycle_group(int group) {
  m_control->cycle_group(group);
}

void
TrackerManager::randomize() {
  m_control->randomize();
}

TrackerManager::size_type
TrackerManager::size() const {
  return m_control->size();
}

TrackerManager::size_type
TrackerManager::group_size() const {
  if (m_control->rbegin() == m_control->rend())
    return 0;
  else
    return (*m_control->rbegin())->group() + 1;
}

TrackerManager::value_type
TrackerManager::get(size_type idx) const {
  return (*m_control)[idx];
}

TrackerManager::size_type
TrackerManager::focus_index() const {
  return std::distance(m_control->begin(), m_control->focus());
}

void
TrackerManager::insert(int group, const std::string& url) {
  if (m_control->has_active())
    throw internal_error("Tried to add tracker while a tracker is active.");

  Tracker* t;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0)
    t = new TrackerHttp(m_control, url);

  else if (std::strncmp("udp://", url.c_str(), 6) == 0)
    t = new TrackerUdp(m_control, url);

  else
    // TODO: Error message here?... not really...
    return;
  
  m_control->insert(group, t);
}

DownloadInfo*
TrackerManager::info() {
  return m_control->info();
}

const DownloadInfo*
TrackerManager::info() const {
  return m_control->info();
}

void
TrackerManager::set_info(DownloadInfo* info) {
  m_control->set_info(info);
}

void
TrackerManager::receive_timeout() {
  if (m_control->has_active())
    throw internal_error("TrackerManager::receive_timeout() called but m_control->has_active() == true.");

  if (!m_active)
    return;

  m_control->send_state((DownloadInfo::State)m_control->state());
}

void
TrackerManager::receive_success(AddressList* l) {
  m_failedRequests = 0;

  if (m_control->state() == DownloadInfo::STOPPED || !m_active)
    return m_slotSuccess(l);

  if (m_control->state() == DownloadInfo::STARTED)
    m_initialTracker = std::distance(m_control->begin(), m_control->focus());

  // Don't reset the focus when we're requesting more peers. If we
  // want to query the next tracker in the list we need to remember
  // the current focus.
  if (m_isRequesting) {
    m_numRequests++;
  } else {
    m_numRequests = 1;
    m_control->set_focus(m_control->begin());
  }

  // Reset m_isRequesting so a new call to request_*() is needed to
  // try from the rest of the trackers in the list. If not called, the
  // next tracker request will reset the focus to the first tracker.
  m_isRequesting = false;

  m_control->set_state(DownloadInfo::NONE);
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(m_control->focus_normal_interval())).round_seconds());

  m_slotSuccess(l);
}

void
TrackerManager::receive_failed(const std::string& msg) {
  if (m_control->state() == DownloadInfo::STOPPED || !m_active)
    return m_slotFailed(msg);

  if (m_isRequesting) {
    // Currently trying to request additional peers.

    if (m_control->focus() == m_control->end()) {
      // Don't start from the beginning of the list if we've gone
      // through the whole list. Return to normal timeout.
      m_isRequesting = false;
      priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(m_control->focus_normal_interval())).round_seconds());
    } else {
      priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(20)).round_seconds());
    }

  } else {
    // Normal retry.

    if (m_control->focus() == m_control->end()) {
      // Tried all the trackers, start from the beginning.
      m_failedRequests++;
      m_control->set_focus(m_control->begin());
    }
    
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(std::min<uint32_t>(600, 20 + 20 * m_failedRequests))).round_seconds());
  }

  m_slotFailed(msg);
}

}
