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

#include <numeric>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "socket_address.h"

namespace torrent {

bool
SocketAddress::set_hostname(const std::string& hostname) {
  hostent* he = gethostbyname(hostname.c_str());

  if (he == NULL)
    return false;

  std::memcpy(&m_sockaddr.sin_addr, he->h_addr_list[0], sizeof(in_addr));

  return true;
}

uint16_t
SocketAddress::get_port() const {
  return ntohs(m_sockaddr.sin_port);
}

void
SocketAddress::set_port(int port) {
  if (port < 0 || port >= (1 << 16))
    throw internal_error("SocketAddress::set_port(...) received an invalid port");

  m_sockaddr.sin_port = htons(port);
}

std::string
SocketAddress::get_address() const {
  return inet_ntoa(m_sockaddr.sin_addr);
}

bool
SocketAddress::set_address(const std::string& addr) {
  if (!addr.empty()) {
    return inet_aton(addr.c_str(), &m_sockaddr.sin_addr);

  } else {
    m_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    return true;
  }
}

bool
SocketAddress::create(const std::string& addr, int port) {
  if (port < 0 || port >= (1 << 16))
    return false;

  m_sockaddr.sin_port = htons(port);
  
  return set_address(addr);
}  

}
