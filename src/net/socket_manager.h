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

// Add some helpfull words here.

#ifndef LIBTORRENT_NET_SOCKET_MANAGER_H
#define LIBTORRENT_NET_SOCKET_MANAGER_H

#include <inttypes.h>

class sockaddr;

namespace torrent {

class SocketManager {
public:
  SocketManager();
  ~SocketManager();
  
  // Check that we have not surpassed the max number of open sockets
  // and that we're allowed to connect to the socket address.
  //
  // Consider only checking max number of open sockets.
  bool                can_connect(const sockaddr* sa);

  // Call this to keep the socket count up to date.
  void                inc_socket_count()            { m_size++; }
  void                dec_socket_count()            { m_size--; }

  // size_type
  uint32_t            size() const                  { return m_size; }

  uint32_t            max_size() const              { return m_max; }
  void                set_max_size(uint32_t s)      { m_max = s; }

  // Propably going to have to make m_bindAddress a pointer to make it
  // safe.
  //
  // Perhaps add length parameter.
  //
  // Setting the bind address makes a copy.
  const sockaddr*     bind_address()                { return m_bindAddress; }
  void                set_bind_address(const sockaddr* sa);

private:
  SocketManager(const SocketManager&);
  void operator = (const SocketManager&);

  uint32_t            m_size;
  uint32_t            m_max;

  sockaddr*           m_bindAddress;
};

}

#endif

