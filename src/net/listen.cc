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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "listen.h"
#include "socket_address.h"
#include "torrent/exceptions.h"

namespace torrent {

bool
Listen::open(uint16_t first, uint16_t last, const std::string& addr) {
  close();

  if (first == 0 || last == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range");

  SocketAddress sa;

  if (!sa.set_address(addr))
    throw local_error("Could not parse the ip to bind the listening socket to");

  if (!m_fd.open() || !m_fd.set_nonblock())
    throw local_error("Could not allocate socket for listening");

  for (uint16_t i = first; i <= last; ++i) {
    sa.set_port(i);

    if (m_fd.bind(sa) && m_fd.listen(50)) {
      m_port = i;

      insert_read();
      insert_except();

      return true;
    }
  }

  m_fd.close();
  m_fd.clear();

  return false;
}

void Listen::close() {
  if (!m_fd.is_valid())
    return;

  m_fd.close();
  m_fd.clear();
  
  m_port = 0;

  remove_read();
  remove_except();
}
  
void Listen::read() {
  SocketAddress sa;
  SocketFd fd;

  while ((fd = m_fd.accept(sa)).is_valid())
    m_slotIncoming(fd.get_fd(), sa.get_address(), sa.get_port());
}

void Listen::write() {
  throw internal_error("Listener does not support write()");
}

void Listen::except() {
  throw local_error("Listener port recived exception");
}

}
