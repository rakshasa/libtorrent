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

#include "torrent/exceptions.h"

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

  m_timeLastTick = cachedTime;

  m_taskTick.set_slot(rak::mem_fn(this, &ThrottleManager::receive_tick));
}

ThrottleManager::~ThrottleManager() {
  priority_queue_erase(&taskScheduler, &m_taskTick);
  delete m_throttleList;
}

void
ThrottleManager::set_max_rate(uint32_t v) {
  if (v == m_maxRate)
    return;

  uint32_t oldRate = m_maxRate;
  m_maxRate = v;

  m_throttleList->set_min_chunk_size(calculate_min_chunk_size());
  m_throttleList->set_max_chunk_size(calculate_max_chunk_size());

  if (oldRate == 0) {
    m_throttleList->enable();

    // We need to start the ticks, and make sure we set m_timeLastTick
    // to a value that gives an reasonable initial quota.
    m_timeLastTick = cachedTime - rak::timer::from_seconds(1);
    receive_tick();

  } else if (m_maxRate == 0) {
    m_throttleList->disable();
    priority_queue_erase(&taskScheduler, &m_taskTick);
  }
}

void
ThrottleManager::receive_tick() {
  if (cachedTime <= m_timeLastTick + 90000)
    throw internal_error("ThrottleManager::receive_tick() called at a to short interval.");

  float timeSinceLast = (cachedTime.usec() - m_timeLastTick.usec()) / 1000000.0f;

  m_throttleList->update_quota((uint32_t)(m_maxRate * timeSinceLast));

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime + calculate_interval());
  m_timeLastTick = cachedTime;
}

uint32_t
ThrottleManager::calculate_min_chunk_size() const {
  // Just for each modification, make this into a function, rather
  // than if-else chain.
  if (m_maxRate <= (8 << 10))
    return (1 << 9);

  else if (m_maxRate <= (32 << 10))
    return (2 << 9);

  else if (m_maxRate <= (64 << 10))
    return (3 << 9);

  else if (m_maxRate <= (128 << 10))
    return (4 << 9);

  else if (m_maxRate <= (512 << 10))
    return (8 << 9);

  else if (m_maxRate <= (2048 << 10))
    return (16 << 9);

  else
    return (32 << 9);
}

uint32_t
ThrottleManager::calculate_max_chunk_size() const {
  // Make this return a lower value for very low throttle settings.
  return calculate_min_chunk_size() * 4;
}

uint32_t
ThrottleManager::calculate_interval() const {
  uint32_t rate = m_throttleList->rate_slow()->rate();

  if (rate < 1024)
    return 10 * 100000;

  // At least two max chunks per tick.
  uint32_t interval = (5 * m_throttleList->max_chunk_size()) / rate;

  if (interval == 0)
    return 1 * 100000;
  else if (interval > 10)
    return 10 * 100000;
  else
    return interval * 100000;
}

}
