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

#ifndef LIBTORRENT_NET_SOCKET_MANAGER_H
#define LIBTORRENT_NET_SOCKET_MANAGER_H

#include <inttypes.h>
#include "socket_fd.h"

namespace torrent {

class SocketAddress;

// Socket manager keeps tabs on how many open sockets we got and helps
// with opening addresses. It will also make sure ip address filtering
// gets handled.
//
// It closes the opened/received socket if the connection is unwanted.
//
// TODO: Rename received to receive.

class SocketManager {
public:
  SocketManager() : m_size(0), m_max(0) {}
  
  SocketFd            open(const SocketAddress& sa, const SocketAddress& b);
  SocketFd            received(SocketFd fd, const SocketAddress& sa);
  void                local(__UNUSED SocketFd fd)   { m_size++; }

  void                close(SocketFd fd);

  uint32_t            size() const                  { return m_size; }

  uint32_t            max_size() const              { return m_max; }
  void                set_max_size(uint32_t s)      { m_max = s; }

private:
  uint32_t            m_size;
  uint32_t            m_max;
};

}

#endif

