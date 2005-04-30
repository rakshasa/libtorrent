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

#ifndef LIBTORRENT_UTILS_THROTTLE_NODE_H
#define LIBTORRENT_UTILS_THROTTLE_NODE_H

#include <inttypes.h>

namespace torrent {

template <typename _Op>
class ThrottleNode {
public:
  enum {
    UNLIMITED = -1
  };

  ThrottleNode(const _Op& o) : m_quota(UNLIMITED), m_used(0), m_op(o) {}
  
  bool                is_unlimited() const          { return m_quota == UNLIMITED; }

  int                 get_quota() const             { return m_quota; }
  void                set_quota(int v)              { m_quota = v; }

  int                 get_used() const              { return m_used; }
  void                set_used(int v)               { m_used = v; }

  void                set_op(const _Op& op)         { m_op = op; }

  void                used(int v)                   { m_used += v; if (!is_unlimited()) m_quota -= v; }
  void                activate()                    { m_op(); }

private:
  int                 m_quota;
  int                 m_used;

  _Op                 m_op;
};

}

#endif
