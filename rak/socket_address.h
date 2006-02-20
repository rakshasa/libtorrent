// rak - Rakshasa's toolbox
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

// Wrappers for the various sockaddr types with focus on zero-copy
// casting between the original type and the wrapper class.
//
// The default ctor does not initialize any data.

// Add define for inet6 scope id?

#ifndef RAK_SOCKET_ADDRESS_H
#define RAK_SOCKET_ADDRESS_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace rak {

class socket_address_inet;
class socket_address_inet6;

class socket_address {
public:
  static const sa_family_t af_local  = AF_LOCAL;
  static const sa_family_t af_unix   = AF_UNIX;
  static const sa_family_t af_file   = AF_FILE;
  static const sa_family_t af_inet   = AF_INET;
  static const sa_family_t af_inet6  = AF_INET6;
  static const sa_family_t af_unspec = AF_UNSPEC;

  sa_family_t         family() const                          { return m_sa.m_sockaddr.sa_family; }
  void                set_family(sa_family_t f)               { m_sa.m_sockaddr.sa_family = f; }

  socket_address_inet*        sa_inet()                       { return reinterpret_cast<socket_address_inet*>(this); }
  socket_address_inet6*       sa_inet6()                      { return reinterpret_cast<socket_address_inet6*>(this); }

  const socket_address_inet*  sa_inet() const                 { return reinterpret_cast<const socket_address_inet*>(this); }
  const socket_address_inet6* sa_inet6() const                { return reinterpret_cast<const socket_address_inet6*>(this); }

  sockaddr*           c_sockaddr()                            { return &m_sa.m_sockaddr; }
  sockaddr_in*        c_sockaddr_inet()                       { return &m_sa.m_sockaddrInet; }
  sockaddr_in6*       c_sockaddr_inet6()                      { return &m_sa.m_sockaddrInet6; }

  const sockaddr*     c_sockaddr() const                      { return &m_sa.m_sockaddr; }
  const sockaddr_in*  c_sockaddr_inet() const                 { return &m_sa.m_sockaddrInet; }
  const sockaddr_in6* c_sockaddr_inet6() const                { return &m_sa.m_sockaddrInet6; }

private:
  union sa_union {
    sockaddr            m_sockaddr;
    sockaddr_in         m_sockaddrInet;
    sockaddr_in6        m_sockaddrInet6;
  };

  sa_union            m_sa;
};

// Remeber to set the AF_INET.

class socket_address_inet {
public:
  socket_address_inet() { }
  socket_address_inet(const sockaddr_in& src) : m_sockaddr(src) {}
  
  bool                is_any() const                          { return is_port_any() && is_address_any(); }
  bool                is_valid() const                        { return !is_port_any() && !is_address_any(); }
  bool                is_port_any() const                     { return port() == 0; }
  bool                is_address_any() const                  { return m_sockaddr.sin_addr.s_addr == htonl(INADDR_ANY); }

  uint16_t            port() const                            { return ntohs(m_sockaddr.sin_port); }
  void                set_port(uint16_t p)                    { m_sockaddr.sin_port = htons(p); }

  // Should address() return the uint32_t?
  in_addr             address() const                         { return m_sockaddr.sin_addr; }
  uint32_t            address_int() const                     { return ntohl(m_sockaddr.sin_addr.s_addr); }
  std::string         address_str() const;
  bool                address_c_str(char* buf, socklen_t size) const;

  void                set_address(in_addr a)                  { m_sockaddr.sin_addr = a; }
  void                set_address_int(uint32_t a)             { m_sockaddr.sin_addr.s_addr = htonl(a); }
  bool                set_address_str(const std::string& a)   { return set_address_c_str(a.c_str()); }
  bool                set_address_c_str(const char* a);

  void                set_address_any()                       { set_port(0); set_address_int(INADDR_ANY); }

  sa_family_t         family() const                          { return m_sockaddr.sin_family; }
  void                set_family()                            { m_sockaddr.sin_family = AF_INET; }

  sockaddr*           c_sockaddr()                            { return reinterpret_cast<sockaddr*>(&m_sockaddr); }
  sockaddr_in*        c_sockaddr_inet()                       { return &m_sockaddr; }

  const sockaddr*     c_sockaddr() const                      { return reinterpret_cast<const sockaddr*>(&m_sockaddr); }
  const sockaddr_in*  c_sockaddr_inet() const                 { return &m_sockaddr; }

  bool                operator == (const socket_address_inet& sa) const;
  bool                operator < (const socket_address_inet& sa) const;

private:
  struct sockaddr_in  m_sockaddr;
};

inline std::string
socket_address_inet::address_str() const {
  char buf[INET_ADDRSTRLEN];

  if (!address_c_str(buf, INET_ADDRSTRLEN))
    return std::string();

  return std::string(buf);
}

inline bool
socket_address_inet::address_c_str(char* buf, socklen_t size) const {
  return inet_ntop(m_sockaddr.sin_family, &m_sockaddr.sin_addr, buf, size);
}

inline bool
socket_address_inet::set_address_c_str(const char* a) {
  return inet_pton(AF_INET, a, &m_sockaddr.sin_addr);
}

inline bool
socket_address_inet::operator == (const socket_address_inet& sa) const {
  return
    m_sockaddr.sin_addr.s_addr == sa.m_sockaddr.sin_addr.s_addr &&
    m_sockaddr.sin_port == sa.m_sockaddr.sin_port;
}

inline bool
socket_address_inet::operator < (const socket_address_inet& sa) const {
  return
    m_sockaddr.sin_addr.s_addr < sa.m_sockaddr.sin_addr.s_addr ||
    (m_sockaddr.sin_addr.s_addr == sa.m_sockaddr.sin_addr.s_addr &&
     m_sockaddr.sin_port < sa.m_sockaddr.sin_port);
}

}

#endif
