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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_NET_THROTTLE_NODE_H
#define LIBTORRENT_NET_THROTTLE_NODE_H

#include <inttypes.h>

namespace torrent {

template <typename _Op>
class ThrottleNode {
public:
  ThrottleNode(const _Op& o) : m_quota(0), m_used(0), m_op(o) {}
  
  uint32_t            get_quota() const                       { return m_quota; }
  uint32_t            get_quota_left() const                  { return m_quota - m_used; }

  void                set_quota(uint32_t v)                   { m_quota = v; }
  void                use_quota(uint32_t v)                   { m_quota -= v; }

  void                set_used(uint32_t v)                    { m_used = v; }

  void                set_op_activate(const _Op& op)          { m_op = op; }

private:
  uint32_t            m_quota;
  uint32_t            m_used;

  _Op                 m_op;
};

}

#endif
