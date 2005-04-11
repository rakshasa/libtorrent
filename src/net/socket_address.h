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

#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <cstring>
#include <sys/socket.h>

namespace torrent {

class SocketAddress {
public:
  SocketAddress();
  explicit SocketAddress(const sockaddr_in& sa);

  size_t              get_sizeof()                            { return sizeof(sockaddr_in); }

  sockaddr_in&        get_addr_in()                           { return m_sockaddr; }
  sockaddr&           get_addr()                              { return (sockaddr&)m_sockaddr; }

  bool                set_host(const std::string& hostname);
  void                set_port(int port);

  bool                create(const std::string& hostname, int port);

private:
  sockaddr_in         m_sockaddr;
};

inline SocketAddress::SocketAddress() {
  std::memset(&m_sockaddr, 0, sizeof(sockaddr_in));
  m_sockaddr.sin_family = AF_INET;
}

inline SocketAddress::SocketAddress(const sockaddr_in& sa) {
  std::memcpy(&m_sockaddr, &sa, sizeof(sockaddr_in));
}

}

#endif
