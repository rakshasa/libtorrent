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

#ifndef LIBTORRENT_NET_POLL_SELECT_H
#define LIBTORRENT_NET_POLL_SELECT_H

#include <sys/select.h>
#include <sys/types.h>

#include "socket_base.h"

namespace torrent {

template <typename _Operation>
struct poll_check_t {
  poll_check_t(fd_set* s, _Operation op) : m_set(s), m_op(op) {}

  void operator () (SocketBase* s) {
//     if (s == NULL)
//       throw internal_error("poll_mark: s == NULL");

    if (FD_ISSET(s->get_fd().get_fd(), m_set))
      m_op(s);
  }

  fd_set*    m_set;
  _Operation m_op;
};

template <typename _Operation>
inline poll_check_t<_Operation>
poll_check(fd_set* s, _Operation op) {
  return poll_check_t<_Operation>(s, op);
}

struct poll_mark {
  poll_mark(fd_set* s, int* m) : m_max(m), m_set(s) {}

  void operator () (SocketBase* s) {
//     if (s == NULL)
//       throw internal_error("poll_mark: s == NULL");

    *m_max = std::max(*m_max, s->get_fd().get_fd());

    FD_SET(s->get_fd().get_fd(), m_set);
  }

  int*    m_max;
  fd_set* m_set;
};

}

#endif
