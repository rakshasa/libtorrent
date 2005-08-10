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

#include <sigc++/hide.h>

#include "torrent/exceptions.h"

#include "tracker_control.h"
#include "tracker_manager.h"

namespace torrent {

// When m_taskTimeout is scheduled then TrackerManager is active and
// m_control is not trying to connect to any tracker. Only if both
// m_taskTimout is unscheduled and m_control is not busy, is
// TrackerManager inactive.

TrackerManager::TrackerManager() :
  m_control(new TrackerControl) {

  m_control->signal_success().connect(sigc::hide(sigc::mem_fun(*this, &TrackerManager::receive_success)));
  m_control->signal_failed().connect(sigc::hide(sigc::mem_fun(*this, &TrackerManager::receive_failed)));

  m_taskTimeout.set_iterator(taskScheduler.end());
  m_taskTimeout.set_slot(sigc::mem_fun(*this, &TrackerManager::receive_timeout));
}

TrackerManager::~TrackerManager() {
  taskScheduler.erase(&m_taskTimeout);
  delete m_control;
}

bool
TrackerManager::is_active() const {
  return taskScheduler.is_scheduled(&m_taskTimeout) || m_control->is_busy();
}

void
TrackerManager::send_start() {
  taskScheduler.erase(&m_taskTimeout);

  m_control->cancel();
  m_control->set_focus_index(0);
  m_control->send_state(TrackerInfo::STARTED);
}

void
TrackerManager::send_stop() {
  if (!is_active())
    return;

  taskScheduler.erase(&m_taskTimeout);
  m_control->send_state(TrackerInfo::STOPPED);
}

void
TrackerManager::send_completed() {
  if (!is_active() || m_control->get_state() == TrackerInfo::STOPPED)
    return;

  taskScheduler.erase(&m_taskTimeout);
  m_control->send_state(TrackerInfo::COMPLETED);
}

void
TrackerManager::request_current() {
  // Keep track of how many times we've requested from the current
  // tracker without waiting for some minimum interval.
  manual_request(true);
}

bool
TrackerManager::request_next() {
  // Check next against last successfull connection?
  return false;
}

void
TrackerManager::manual_request(bool force) {
  if (!taskScheduler.is_scheduled(&m_taskTimeout))
    return;

  Timer t(Timer::cache() + 2 * 1000000);
  
  if (!force)
    t = std::max(t, m_control->time_last_connection() + m_control->get_min_interval() * 1000000);

  taskScheduler.erase(&m_taskTimeout);
  taskScheduler.insert(&m_taskTimeout, t);
}

Timer
TrackerManager::get_next_timeout() const {
  if (taskScheduler.is_scheduled(&m_taskTimeout))
    return m_taskTimeout.get_time();
  else
    return m_control->get_next_time();
}

void
TrackerManager::receive_timeout() {
  if (m_control->is_busy())
    throw internal_error("TrackerManager::receive_timeout() called but m_control->is_busy() == true.");

  m_control->cancel();
  m_control->set_focus_index(0);
  m_control->send_state(TrackerInfo::NONE);
}

void
TrackerManager::receive_success() {
  if (m_control->get_state() != TrackerInfo::STOPPED)
    taskScheduler.insert(&m_taskTimeout, Timer::cache() + m_control->get_normal_interval() * 1000000);

  m_control->set_focus_index(0);
}

void
TrackerManager::receive_failed() {
  // Check if focus == end to see if we've gone through all trackers.
}

}
