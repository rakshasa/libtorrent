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

#include <rak/functional.h>
#include <rak/timer.h> 
#include <rak/priority_queue_default.h> 

#include "net/throttle_internal.h"
#include "net/throttle_list.h"
#include "torrent/exceptions.h"
#include "torrent/throttle.h"

#include "globals.h"

namespace torrent {

// Plans:
//
// Make ThrottleList do a callback when it needs more? This would
// allow us to remove us from the task scheduler when we're full. Also
// this would let us be abit more flexible with the interval.

ThrottleInternal::ThrottleInternal(int flags) :
  m_flags(flags),
  m_nextSlave(m_slaveList.end()),
  m_unusedQuota(0),
  m_timeLastTick(cachedTime) {

  if (is_root())
    m_taskTick.slot() = std::bind(&ThrottleInternal::receive_tick, this);
}

ThrottleInternal::~ThrottleInternal() {
  if (is_root())
    priority_queue_erase(&taskScheduler, &m_taskTick);

  std::for_each(m_slaveList.begin(), m_slaveList.end(), rak::call_delete<ThrottleInternal>());
}

void
ThrottleInternal::enable() {
  m_throttleList->enable();
  std::for_each(m_slaveList.begin(), m_slaveList.end(), std::mem_fun(&ThrottleInternal::enable));

  if (is_root()) {
    // We need to start the ticks, and make sure we set timeLastTick
    // to a value that gives an reasonable initial quota.
    m_timeLastTick = cachedTime - rak::timer::from_seconds(1);
    receive_tick();
  }
}

void
ThrottleInternal::disable() {
  m_throttleList->disable();
  std::for_each(m_slaveList.begin(), m_slaveList.end(), std::mem_fun(&ThrottleInternal::disable));

  if (is_root())
    priority_queue_erase(&taskScheduler, &m_taskTick);
}

ThrottleInternal*
ThrottleInternal::create_slave() {
  ThrottleInternal* slave = new ThrottleInternal(flag_none);

  slave->m_maxRate = m_maxRate;
  slave->m_throttleList = new ThrottleList();

  if (m_throttleList->is_enabled())
    slave->enable();

  m_slaveList.push_back(slave);
  m_nextSlave = m_slaveList.end();

  return slave;
}

void
ThrottleInternal::receive_tick() {
  if (cachedTime <= m_timeLastTick + rak::timer::from_milliseconds(90))
    throw internal_error("ThrottleInternal::receive_tick() called at a to short interval.");

  uint32_t quota = ((uint64_t)(cachedTime.usec() - m_ptr()->m_timeLastTick.usec())) * m_maxRate / 1000000;
  uint32_t fraction = ((uint64_t)(cachedTime.usec() - m_ptr()->m_timeLastTick.usec())) * fraction_base / 1000000;

  receive_quota(quota, fraction);

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime + calculate_interval());
  m_timeLastTick = cachedTime;
}

int32_t
ThrottleInternal::receive_quota(uint32_t quota, uint32_t fraction) {
  uint32_t need;

  m_unusedQuota += quota;

  while (m_nextSlave != m_slaveList.end()) {
    need = std::min<uint32_t>(quota, (uint64_t)fraction * (*m_nextSlave)->max_rate() >> fraction_bits);
    if (m_unusedQuota < need)
      break;

    m_unusedQuota -= (*m_nextSlave)->receive_quota(need, fraction);
    m_throttleList->add_rate((*m_nextSlave)->throttle_list()->rate_added());
    ++m_nextSlave;
  }

  need = std::min<uint32_t>(quota, (uint64_t)fraction * m_maxRate >> fraction_bits);
  if (m_nextSlave == m_slaveList.end() && m_unusedQuota >= need) {
    m_unusedQuota -= m_throttleList->update_quota(need);
    m_nextSlave = m_slaveList.begin();
  }

  // Return how much quota we used, but keep as much as  one
  // allocation's worth until the next tick to avoid rounding errors.
  int32_t used = quota;

  if (m_unusedQuota > quota) {
    used -= m_unusedQuota - quota;
    m_unusedQuota = quota;
  }

  // Amount used may be negative if rate changed between last tick and now.
  return used;
}

}
