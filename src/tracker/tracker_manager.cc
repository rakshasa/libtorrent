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

#include "torrent/exceptions.h"

#include "tracker_control.h"
#include "tracker_manager.h"

namespace torrent {

TrackerManager::TrackerManager() :
  m_control(new TrackerControl),
  m_isRequesting(false),
  m_numRequests(0),
  m_maxRequests(3),
  m_initialTracker(0) {

  m_control->slot_success(rak::make_mem_fun(this, &TrackerManager::receive_success));
  m_control->slot_failed(rak::make_mem_fun(this, &TrackerManager::receive_failed));

  m_taskTimeout.set_slot(rak::mem_fn(this, &TrackerManager::receive_timeout));
}

TrackerManager::~TrackerManager() {
  if (is_active())
    throw internal_error("TrackerManager::~TrackerManager() called but is_active() != false.");

  delete m_control;
}

bool
TrackerManager::is_active() const {
  return m_taskTimeout.is_queued() || m_control->is_busy();
}

bool
TrackerManager::is_busy() const {
  return m_control->is_busy();
}

void
TrackerManager::close() {
  m_isRequesting = false;

  m_control->close();
  taskScheduler.erase(m_taskTimeout.clear());
}

void
TrackerManager::send_start() {
  close();

  m_control->set_focus_index(0);
  m_control->send_state(TrackerInfo::STARTED);
}

void
TrackerManager::send_stop() {
  if (!is_active())
    return;

  close();

  m_control->set_focus_index(m_initialTracker);
  m_control->send_state(TrackerInfo::STOPPED);
}

void
TrackerManager::send_completed() {
  if (!is_active() || m_control->get_state() == TrackerInfo::STOPPED)
    return;

  close();
  m_control->send_state(TrackerInfo::COMPLETED);
}

// When request_{current,next} is called, m_isRequesting is set to
// true. This ensures that if none of the remaining trackers can be
// reached or if a connection is successfull, it will not reset the
// focus to the first tracker.
//
// The client can therefor call these functions after
// TrackerControl::signal_success is emited and know it won't cause
// looping if there are unreachable trackers.
//
// When the number of consequtive requests from the same tracker
// through this function has reached a certain limit, it will stop the
// request. 'm_maxRequests' thus makes sure that a client with a very
// high "min peers" setting will not cause too much traffic.
bool
TrackerManager::request_current() {
  if (m_control->is_busy() || m_numRequests >= m_maxRequests)
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
  if (m_control->is_busy() || !m_control->focus_next_group())
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

  rak::timer t(cachedTime + 2 * 1000000);
  
  if (!force)
    t = std::max(t, m_control->time_last_connection() + m_control->get_min_interval() * 1000000);

  taskScheduler.erase(m_taskTimeout.clear());
  taskScheduler.push(m_taskTimeout.prepare(t.round_seconds()));
}

void
TrackerManager::cycle_group(int group) {
  m_control->cycle_group(group);
}

void
TrackerManager::randomize() {
  m_control->get_list().randomize();
}

TrackerManager::size_type
TrackerManager::size() const {
  return m_control->get_list().size();
}

TrackerManager::size_type
TrackerManager::group_size() const {
  if (m_control->get_list().rbegin() == m_control->get_list().rend())
    return 0;
  else
    return m_control->get_list().rbegin()->first + 1;
}

TrackerManager::value_type
TrackerManager::get(size_type idx) const {
  return m_control->get_list()[idx];
}

TrackerManager::size_type
TrackerManager::focus_index() const {
  return m_control->focus_index();
}

void
TrackerManager::insert(int group, const std::string& url) {
  // Consider borking m_initialTracker.

  m_control->insert(group, url);
}

TrackerInfo*
TrackerManager::tracker_info() {
  return m_control->get_info();
}

void
TrackerManager::receive_timeout() {
  if (m_control->is_busy())
    throw internal_error("TrackerManager::receive_timeout() called but m_control->is_busy() == true.");

  m_control->send_state(m_control->get_state());
}

void
TrackerManager::receive_success(AddressList* l) {
  if (m_control->get_state() == TrackerInfo::STOPPED) {
    m_slotSuccess(l);
    return;
  }

  if (m_control->get_state() == TrackerInfo::STARTED)
    m_initialTracker = m_control->focus_index();

  // Don't reset the focus when we're requesting more peers. If we
  // want to query the next tracker in the list we need to remember
  // the current focus.
  if (m_isRequesting) {
    m_numRequests++;
  } else {
    m_numRequests = 1;
    m_control->set_focus_index(0);
  }

  // Reset m_isRequesting so a new call to request_*() is needed to
  // try from the rest of the trackers in the list. If not called, the
  // next tracker request will reset the focus to the first tracker.
  m_isRequesting = false;

  m_control->set_state(TrackerInfo::NONE);
  taskScheduler.push(m_taskTimeout.prepare((cachedTime + m_control->get_normal_interval() * 1000000).round_seconds()));

  m_slotSuccess(l);
}

void
TrackerManager::receive_failed(const std::string& msg) {
  if (m_control->get_state() == TrackerInfo::STOPPED) {
    m_slotFailed(msg);
    return;
  }

  if (m_isRequesting) {
    if (m_control->focus_index() == m_control->get_list().size()) {
      // Don't start from the beginning of the list if we've gone
      // through the whole list. Return to normal timeout.
      m_isRequesting = false;
      taskScheduler.push(m_taskTimeout.prepare((cachedTime + m_control->get_normal_interval() * 1000000).round_seconds()));
    } else {
      taskScheduler.push(m_taskTimeout.prepare((cachedTime + 20 * 1000000).round_seconds()));
    }

  } else {
    // Normal retry.
    if (m_control->focus_index() == m_control->get_list().size())
      m_control->set_focus_index(0);
    
    taskScheduler.push(m_taskTimeout.prepare((cachedTime + 20 * 1000000).round_seconds()));
  }

  m_slotFailed(msg);
}

}
