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

#include <rak/timer.h> 

#include "net/throttle_internal.h"
#include "net/throttle_list.h"

#include "globals.h"

#include "exceptions.h"
#include "throttle.h"

namespace torrent {

// Plans:
//
// Make ThrottleList do a callback when it needs more? This would
// allow us to remove us from the task scheduler when we're full. Also
// this would let us be abit more flexible with the interval.

Throttle*
Throttle::create_throttle() {
  ThrottleInternal* throttle = new ThrottleInternal(ThrottleInternal::flag_root);

  throttle->m_maxRate = 0;
  throttle->m_throttleList = new ThrottleList();

  return throttle;
}

void
Throttle::destroy_throttle(Throttle* throttle) {
  delete throttle->m_ptr()->m_throttleList;
  delete throttle->m_ptr();
}

Throttle*
Throttle::create_slave() {
  return m_ptr()->create_slave();
}

bool
Throttle::is_throttled() {
  return m_maxRate != 0 && m_maxRate != std::numeric_limits<uint32_t>::max();
}

void
Throttle::set_max_rate(uint32_t v) {
  if (v == m_maxRate)
    return;

  if (v > (1 << 30))
    throw input_error("Throttle rate must be between 0 and 2^30.");

  uint32_t oldRate = m_maxRate;
  m_maxRate = v;

  m_throttleList->set_min_chunk_size(calculate_min_chunk_size());
  m_throttleList->set_max_chunk_size(calculate_max_chunk_size());

  if (!m_ptr()->is_root())
    return;

  if (oldRate == 0)
    m_ptr()->enable();
  else if (m_maxRate == 0)
    m_ptr()->disable();
}

const Rate*
Throttle::rate() const {
  return m_throttleList->rate_slow();
}

uint32_t
Throttle::calculate_min_chunk_size() const {
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
Throttle::calculate_max_chunk_size() const {
  // Make this return a lower value for very low throttle settings.
  return calculate_min_chunk_size() * 4;
}

uint32_t
Throttle::calculate_interval() const {
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
