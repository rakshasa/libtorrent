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

#ifndef LIBTORRENT_NET_SOCKET_FD_H
#define LIBTORRENT_NET_SOCKET_FD_H

#include <unistd.h>

namespace torrent {

class SocketAddress;

// By default only allow sockets streams of type AF_INET for the
// moment.

class SocketFd {
public:
  SocketFd() : m_fd(-1) {}
  SocketFd(int fd) : m_fd(fd) {}

  bool                is_valid() const                        { return m_fd >= 0; }
  
  int                 get_fd() const                          { return m_fd; }
  void                set_fd(int fd)                          { m_fd = fd; }

  bool                set_nonblock();
  bool                set_throughput();

  int                 get_error() const;

  bool                open();
  void                close();

  void                clear()                                 { m_fd = -1; }

  bool                bind(const SocketAddress& sa);
  bool                connect(const SocketAddress& sa);

  bool                listen(int size);
  SocketFd            accept(SocketAddress& sa);

private:
  int                 m_fd;
};

}

#endif
