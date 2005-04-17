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

#include <errno.h>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <unistd.h>

#include "socket_address.h"
#include "socket_base.h"
#include "torrent/exceptions.h"
#include "poll.h"

namespace torrent {

SocketBase::~SocketBase() {
  if (m_fd.is_valid())
    throw internal_error("SocketBase::~SocketBase() called but m_fd is still valid");
}

unsigned int
SocketBase::read_buf(void* buf, unsigned int length) {
  if (length == 0)
    throw internal_error("Tried to read buffer length 0");

  errno = 0;
  int r = ::read(m_fd.get_fd(), buf, length);

  if (r == 0)
    throw close_connection();

  else if (r < 0 && errno != EAGAIN && errno != EINTR)
    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));

  return std::max(r, 0);
}

unsigned int
SocketBase::write_buf(const void* buf, unsigned int length) {
  if (length == 0)
    throw internal_error("Tried to write buffer length 0");

  errno = 0;
  int r = ::write(m_fd.get_fd(), buf, length);

  if (r == 0)
    throw close_connection();

  else if (r < 0 && errno != EAGAIN && errno != EINTR)
    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));

  return std::max(r, 0);
}

} // namespace torrent
