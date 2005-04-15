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

SocketBase::Sockets SocketBase::m_readSockets;
SocketBase::Sockets SocketBase::m_writeSockets;
SocketBase::Sockets SocketBase::m_exceptSockets;

SocketBase::~SocketBase() {
  remove_read();
  remove_write();
  remove_except();
}

void SocketBase::insert_read() {
  if (!in_read())
    m_readItr = m_readSockets.insert(m_readSockets.begin(), this);
}

void SocketBase::insert_write() {
  if (!in_write())
    m_writeItr = m_writeSockets.insert(m_writeSockets.begin(), this);
}

void SocketBase::insert_except() {
  if (!in_except())
    m_exceptItr = m_exceptSockets.insert(m_exceptSockets.begin(), this);
}

void SocketBase::remove_read() {
  if (in_read()) {
    m_readSockets.erase(m_readItr);
    m_readItr = m_readSockets.end();
  }
}

void SocketBase::remove_write() {
  if (in_write()) {
    m_writeSockets.erase(m_writeItr);
    m_writeItr = m_writeSockets.end();
  }
}

void SocketBase::remove_except() {
  if (in_except()) {
    m_exceptSockets.erase(m_exceptItr);
    m_exceptItr = m_exceptSockets.end();
  }
}

bool SocketBase::read_buf2(void* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to read socket buffer with wrong length and pos " << length << ' ' << pos;

    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::read(m_fd.get_fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN && errno != EINTR) {

    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

bool SocketBase::write_buf2(const void* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to write socket buffer with wrong length and pos " << length << ' ' << pos;
    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::write(m_fd.get_fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN && errno != EINTR) {

    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

unsigned int SocketBase::read_buf(void* buf, unsigned int length) {
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

unsigned int SocketBase::write_buf(const void* buf, unsigned int length) {
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
