// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include "torrent/net/bind_manager.h"
#include "torrent/utils/log.h"

#include "listen.h"
#include "manager.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen: " log_fmt, __VA_ARGS__);
#define LT_LOG_SA(sa, log_fmt, ...)                                     \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen->%s: " log_fmt, (sa)->pretty_address_str().c_str(), __VA_ARGS__);

// TODO: Use the new pretty_address_str thing.
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen->%s: " log_fmt, rak::socket_address::cast_from(sa)->pretty_address_str().c_str(), __VA_ARGS__);

namespace torrent {

bool
Listen::open(uint16_t first, uint16_t last, int backlog) {
  close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range.");

  auto alloc_fd = []() {
    SocketFd fd;

    if (!fd.open_stream())
      return -1;

    if (!fd.set_nonblock() || !fd.set_reuse_address(true)) {
      fd.close();
      return -1;
    }

    return fd.get_fd();
  };

  auto listen_fd = [this, backlog](int file_desc, const sockaddr* bind_address) {
    SocketFd socket_fd(file_desc);

    if (!socket_fd.listen(backlog)) {
      LT_LOG_SOCKADDR("listen call failed", bind_address, 0);
      return false;
    }

    this->m_port = rak::socket_address::cast_from(bind_address)->port();

    LT_LOG_SOCKADDR("listen port %" PRIu16 " opened with backlog set to %i", bind_address, this->m_port, backlog);
    return true;
  };

  m_fileDesc = manager->bind()->listen_socket(first, last, 0, alloc_fd, listen_fd);

  if (m_fileDesc == -1) {
    get_fd().close();
    get_fd().clear();

    LT_LOG("failed to open listen port", 0);

    return false;
  }

  manager->connection_manager()->inc_socket_count();

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_error(this);

  return true;
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
    m_slot_accepted(fd, sa);
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  int error = get_fd().get_error();

  if (error != 0)
    throw internal_error("Listener port received an error event: " + std::string(std::strerror(error)));
}

}
