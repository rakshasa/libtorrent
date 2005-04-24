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

#include "config.h"

#include "socket_address.h"
#include "socket_manager.h"

namespace torrent {

SocketFd
SocketManager::open(const SocketAddress& sa) {
  SocketFd fd;

  // TODO: Check if there's too many open sockets.
  if (m_size >= m_max || !fd.open())
    return SocketFd();

  if (!fd.set_nonblock() || !fd.connect(sa)) {
    fd.close();
    return SocketFd();
  }

  m_size++;

  return fd;
}

void
SocketManager::close(SocketFd fd) {
  if (!fd.is_valid())
    throw internal_error("SocketManager::close(...) received an invalid file descriptor");

  fd.close();
  m_size--;
}

}
