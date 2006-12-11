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

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rak/socket_address.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/poll.h"

#include "listen.h"
#include "manager.h"

namespace torrent {

bool
Listen::open(uint16_t first, uint16_t last, const rak::socket_address* bindAddress) {
  close();

  if (first == 0 || last == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range.");

  if (bindAddress->family() != rak::socket_address::af_inet &&
      bindAddress->family() != rak::socket_address::af_inet6)
    throw input_error("Listening socket must be bound to an inet or inet6 address.");

  if (!get_fd().open_stream() || !get_fd().set_nonblock())
    throw resource_error("Could not allocate socket for listening.");

  if (!get_fd().set_reuse_address(true))
    throw resource_error("Could not set listening port to reuse address.");

  rak::socket_address sa;
  sa.copy(*bindAddress, bindAddress->length());

  for (uint16_t i = first; i <= last; ++i) {
    sa.set_port(i);

    if (get_fd().bind(sa) && get_fd().listen(50)) {
      m_port = i;

      manager->connection_manager()->inc_socket_count();

      manager->poll()->open(this);
      manager->poll()->insert_read(this);
      manager->poll()->insert_error(this);

      return true;
    }
  }

  // This needs to be done if local_error is thrown too...
  get_fd().close();
  get_fd().clear();

  return false;
}

void Listen::close() {
  if (!get_fd().is_valid())
    return;

  manager->poll()->remove_read(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);

  manager->connection_manager()->dec_socket_count();

  get_fd().close();
  get_fd().clear();
  
  m_port = 0;
}
  
void
Listen::event_read() {
  rak::socket_address sa;
  SocketFd fd;

  while ((fd = get_fd().accept(&sa)).is_valid())
    m_slotIncoming(fd, sa);
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  throw resource_error("Listener port received an error event.");
}

}
