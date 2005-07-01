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

  void                set_op(const _Op& op)         { m_op = op; }

  void                used(int v)                   { m_used += v; if (!is_unlimited()) m_quota -= v; }
  void                activate()                    { m_op(); }

  void                update_quota(int v)           { m_quota = v; m_used = 0; }

private:
  int                 m_quota;
  int                 m_used;

  _Op                 m_op;
};

}

#endif
