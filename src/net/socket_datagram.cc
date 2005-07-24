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

#include "config.h"

#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "socket_address.h"

#include "socket_datagram.h"

namespace torrent {

int
SocketDatagram::send(const void* buffer, unsigned int length, SocketAddress* sa) {
  if (length == 0)
    throw internal_error("Tried to send buffer length 0");

  int r;

  if (sa != NULL) {
    r = ::sendto(m_fileDesc, buffer, length, 0, &sa->get_addr(), sa->get_sizeof());
  } else {
    r = ::send(m_fileDesc, buffer, length, 0);
  }

  if (r < 0)
    m_errno = errno;

  return r;
}

int
SocketDatagram::receive(void* buffer, unsigned int length, SocketAddress* sa) {
  if (length == 0)
    throw internal_error("Tried to receive buffer length 0");

  int r;
  socklen_t fromlen;

  if (sa != NULL) {
    fromlen = sa->get_sizeof();
    r = ::recvfrom(m_fileDesc, buffer, length, 0, &sa->get_addr(), &fromlen);
  } else {
    r = ::recv(m_fileDesc, buffer, length, 0);
  }

  if (r < 0)
    m_errno = errno;

  return r;
}

}
