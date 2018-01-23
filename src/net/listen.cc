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
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#include "listen.h"
#include "manager.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen: " log_fmt, __VA_ARGS__);
#define LT_LOG_SOCKADDR(log_fmt, sa, ...)                               \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen->%s: " log_fmt, sa_pretty_str(sa).c_str(), __VA_ARGS__);

namespace torrent {

bool
Listen::open() {
  close();

  listen_result_type listen_result = manager->bind()->listen_socket(0);
  m_fileDesc = listen_result.fd;
  m_sockaddr.swap(listen_result.sockaddr);

  if (m_fileDesc == -1) {
    LT_LOG("failed to open listen port", 0);
    return false;
  }

  manager->connection_manager()->inc_socket_count();

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_error(this);

  LT_LOG_SOCKADDR("listen port %" PRIu16 " opened", m_sockaddr.get(), sa_port(m_sockaddr.get()));
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
  
  m_sockaddr.release();
}
  
uint16_t
Listen::port() const {
  return m_sockaddr ? sa_port(m_sockaddr.get()) : 0;
}

void
Listen::event_read() {
  rak::socket_address sa;
  SocketFd fd;

  while ((fd = get_fd().accept(&sa)).is_valid()) {
    LT_LOG("accepted connection (fd:%i address:%s)", fd.get_fd(), sa_pretty_str(sa.c_sockaddr()).c_str());

    m_slot_accepted(fd.get_fd(), sa.c_sockaddr());
  }
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  int error = get_fd().get_error();

  LT_LOG("event_error: %s", std::strerror(error));

  if (error != 0)
    throw internal_error("Listener port received an error event: " + std::string(std::strerror(error)));
}

}
