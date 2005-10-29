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

#include "throttle_list.h"
#include "throttle_manager.h"

namespace torrent {

// Plans:
//
// Make ThrottleList do a callback when it needs more? This would
// allow us to remove us from the task scheduler when we're full. Also
// this would let us be abit more flexible with the interval.

ThrottleManager::ThrottleManager() :
  m_maxRate(0),
  m_throttleList(new ThrottleList()) {

  m_timeLastTick = Timer::cache();

  m_taskTick.set_iterator(taskScheduler.end());
  m_taskTick.set_slot(sigc::mem_fun(*this, &ThrottleManager::receive_tick));
}

ThrottleManager::~ThrottleManager() {
  taskScheduler.erase(&m_taskTick);
  delete m_throttleList;
}

void
ThrottleManager::set_max_rate(uint32_t v) {
  if (v == m_maxRate)
    return;

  uint32_t oldRate = m_maxRate;
  m_maxRate = v;

  if (oldRate == 0) {
    m_throttleList->enable();

    // We need to start the ticks, and make sure we set m_timeLastTick
    // to a value that gives an reasonable initial quota.
    m_timeLastTick = Timer::cache() - 1000000;
    receive_tick();

  } else if (m_maxRate == 0) {
    m_throttleList->disable();
    taskScheduler.erase(&m_taskTick);
  }
}

void
ThrottleManager::receive_tick() {
  m_throttleList->update_quota(m_maxRate);

  taskScheduler.insert(&m_taskTick, Timer::cache().round_seconds() + 1000000);
}

}
