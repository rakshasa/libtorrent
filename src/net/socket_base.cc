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

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <unistd.h>

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

void SocketBase::set_socket_nonblock(int fd) {
  // Set non-blocking.
  fcntl(fd, F_SETFL, O_NONBLOCK);
}

void SocketBase::set_socket_min_cost(int fd) {
  // TODO: What of priorities?
//   char opt = ;

//   if (setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &opt, sizeof(opt)))
//     throw local_error("Error setting socket to IPTOS_MINCOST");
}

int SocketBase::get_socket_error(int fd) {
  int err = 0;
  socklen_t length = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &length);

  return err;
}

void SocketBase::make_sockaddr(const std::string& host, int port, sockaddr_in& sa) {
  if (!host.length() ||
      port <= 0 || port >= (1 << 16))
    throw input_error("Tried connecting with invalid hostname or port");

  hostent* he = gethostbyname(host.c_str());

  if (he == NULL)
    throw input_error("Could not lookup host \"" + host + "\"");

  std::memset(&sa, 0, sizeof(sockaddr_in));
  std::memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof(in_addr));

  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
}

int SocketBase::make_socket(sockaddr_in& sa) {
  int f = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (f < 0)
    throw local_error("Could not open socket");

  set_socket_nonblock(f);

  if (connect(f, (sockaddr*)&sa, sizeof(sockaddr_in)) != 0 &&
      errno != EINPROGRESS) {
    close(f);
    throw network_error("Could not connect to remote host");
  }

  return f;
}

void
SocketBase::close_socket(int fd) {
  ::close(fd);
}

bool SocketBase::read_buf(void* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to read socket buffer with wrong length and pos " << length << ' ' << pos;

    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::read(fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN && errno != EINTR) {

    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

bool SocketBase::write_buf(const void* buf, unsigned int length, unsigned int& pos) {
  if (length <= pos) {
    std::stringstream s;
    s << "Tried to write socket buffer with wrong length and pos " << length << ' ' << pos;
    throw internal_error(s.str());
  }

  errno = 0;
  int r = ::write(fd(), buf, length - pos);

  if (r == 0) {
    throw close_connection();

  } else if (r < 0 && errno != EAGAIN && errno != EINTR) {

    throw connection_error(std::string("Connection closed due to ") + std::strerror(errno));
  } else if (r < 0) {
    return false;
  }

  return length == (pos += r);
}

} // namespace torrent
