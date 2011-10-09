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

#ifndef LIBTORRENT_NET_THROTTLE_INTERNAL_H
#define LIBTORRENT_NET_THROTTLE_INTERNAL_H

#include <vector>
#include <rak/priority_queue_default.h>

#include "torrent/common.h"
#include "torrent/throttle.h"

namespace torrent {

class ThrottleInternal : public Throttle {
public:
  static const int flag_none = 0;
  static const int flag_root = 1;

  ThrottleInternal(int flags);
  ~ThrottleInternal();

  ThrottleInternal*   create_slave();

  bool                is_root()         { return m_flags & flag_root; }

  void                enable();
  void                disable();

private:
  // Fraction is a fixed-precision value with the given number of bits after the decimal point.
  static const uint32_t fraction_bits = 16;
  static const uint32_t fraction_base = (1 << fraction_bits);

  typedef std::vector<ThrottleInternal*>  SlaveList;

  void                receive_tick();

  // Distribute quota, return amount of quota used. May be negative
  // if it had more unused quota than is now allowed.
  int32_t             receive_quota(uint32_t quota, uint32_t fraction);

  int                 m_flags;
  SlaveList           m_slaveList;
  SlaveList::iterator m_nextSlave;

  uint32_t            m_unusedQuota;

  rak::timer          m_timeLastTick;
  rak::priority_item  m_taskTick;
};

}

#endif
