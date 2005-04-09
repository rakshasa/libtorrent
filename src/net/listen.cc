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
#include "torrent/exceptions.h"

namespace torrent {

bool Listen::open(uint16_t first, uint16_t last, const std::string& addr) {
  close();

  if (first == 0 || last == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range");

  int fdesc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (fdesc < 0)
    throw local_error("Could not allocate socket for listening");

  sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sockaddr_in));

  sa.sin_family = AF_INET;

  if (!set_sin_addr(sa, addr))
    throw local_error("Could not parse the ip to bind the listening socket to");

  for (uint16_t i = first; i <= last; ++i) {
    sa.sin_port = htons(i);

    if (bind(fdesc, (sockaddr*)&sa, sizeof(sockaddr_in)) == 0) {
      // Opened a port, rejoice.
      m_fd = fdesc;
      m_port = i;

      set_socket_nonblock(m_fd);

      insert_read();
      insert_except();

      // Create cue.
      return ::listen(fdesc, 50) == 0;
    }
  }

  ::close(fdesc);

  return false;
}

void Listen::close() {
  if (m_fd < 0)
    return;

  ::close(m_fd);
  
  m_fd = -1;
  m_port = 0;

  remove_read();
  remove_except();
}
  
void Listen::read() {
  sockaddr_in sa;
  socklen_t sl = sizeof(sockaddr_in);

  int fd;

  while ((fd = accept(m_fd, (sockaddr*)&sa, &sl)) >= 0) {
    m_slotIncoming(fd, inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
  }
}

void Listen::write() {
  throw internal_error("Listener does not support write()");
}

void Listen::except() {
  throw local_error("Listener port recived exception");
}

}
