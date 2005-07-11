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

#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <string>
#include <cstring>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace torrent {

// SocketAddress is by default AF_INET.

class SocketAddress {
public:
  SocketAddress();
  explicit SocketAddress(const sockaddr_in& sa);

  bool                is_any() const                          { return is_port_any() && is_address_any(); }
  bool                is_port_any() const                     { return m_sockaddr.sin_port == 0; }
  bool                is_address_any() const                  { return m_sockaddr.sin_addr.s_addr == htonl(INADDR_ANY); }

  size_t              get_sizeof() const                      { return sizeof(sockaddr_in); }

  sockaddr&           get_addr()                              { return (sockaddr&)m_sockaddr; }
  const sockaddr&     get_addr() const                        { return (sockaddr&)m_sockaddr; }
  sockaddr_in&        get_addr_in()                           { return m_sockaddr; }
  const sockaddr_in&  get_addr_in() const                     { return m_sockaddr; }

  bool                set_hostname(const std::string& hostname);

  uint16_t            get_port() const;
  void                set_port(uint16_t port);

  // Use an empty string for setting INADDR_ANY.
  std::string         get_address() const;
  bool                set_address(const std::string& addr);

  void                set_address_any()                       { m_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); }

  // Uses set_address, so only ip addresses and empty strings are allowed.
  bool                create(const std::string& addr, uint16_t port);

private:
  sockaddr_in         m_sockaddr;
};

inline
SocketAddress::SocketAddress() {
  std::memset(&m_sockaddr, 0, sizeof(sockaddr_in));
  m_sockaddr.sin_family = AF_INET;
  set_address_any();
}

inline
SocketAddress::SocketAddress(const sockaddr_in& sa) {
  std::memcpy(&m_sockaddr, &sa, sizeof(sockaddr_in));
}

}

#endif
