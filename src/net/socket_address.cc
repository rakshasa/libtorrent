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

#include "config.h"

#include <numeric>
#include <netdb.h>
#include <arpa/inet.h>

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
SocketAddress::set_port(uint16_t port) {
  m_sockaddr.sin_port = htons(port);
}

std::string
SocketAddress::get_address() const {
  return inet_ntoa(m_sockaddr.sin_addr);
}

bool
SocketAddress::set_address(const std::string& addr) {
  if (addr.empty()) {
    m_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    return true;

  } else if (inet_aton(addr.c_str(), &m_sockaddr.sin_addr)) {
    return true;

  } else {
    m_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    return false;
  }    
}

bool
SocketAddress::create(const std::string& addr, uint16_t port) {
  m_sockaddr.sin_port = htons(port);
  
  return set_address(addr);
}  

}
