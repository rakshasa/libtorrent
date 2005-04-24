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

#ifndef LIBTORRENT_NET_SOCKET_MANAGER_H
#define LIBTORRENT_NET_SOCKET_MANAGER_H

#include "socket_fd.h"

namespace torrent {

class SocketAddress;

class SocketManager {
public:
  SocketManager() : m_size(0), m_max(0) {}
  
  SocketFd            open(const SocketAddress& sa);
  void                close(SocketFd fd);

  uint32_t            get_size() const              { return m_size; }

  uint32_t            get_max() const               { return m_max; }
  void                set_max(uint32_t s)           { m_max = s; }

private:
  uint32_t            m_size;
  uint32_t            m_max;
};

}

#endif

