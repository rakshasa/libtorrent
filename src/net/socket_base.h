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

#ifndef LIBTORRENT_NET_SOCKET_BASE_H
#define LIBTORRENT_NET_SOCKET_BASE_H

#include <list>

#include "poll.h"
#include "socket_fd.h"

namespace torrent {

class SocketAddress;

class SocketBase {
public:
  typedef std::list<SocketBase*> Sockets;

  SocketBase(SocketFd fd = SocketFd()) : m_fd(fd) {}

  virtual ~SocketBase();

  bool                in_read()           { return Poll::read_set().has(this); }
  bool                in_write()          { return Poll::write_set().has(this); }
  bool                in_except()         { return Poll::except_set().has(this); }

  void                insert_read()       { Poll::read_set().insert(this); }
  void                insert_write()      { Poll::write_set().insert(this); }
  void                insert_except()     { Poll::except_set().insert(this); }

  void                remove_read()       { Poll::read_set().erase(this); }
  void                remove_write()      { Poll::write_set().erase(this); }
  void                remove_except()     { Poll::except_set().erase(this); }

  SocketFd            get_fd()            { return m_fd; }

  virtual void        read() = 0;
  virtual void        write() = 0;
  virtual void        except() = 0;

protected:
  bool                read_buf2(void* buf, unsigned int length, unsigned int& pos);
  bool                write_buf2(const void* buf, unsigned int length, unsigned int& pos);

  unsigned int        read_buf(void* buf, unsigned int length);
  unsigned int        write_buf(const void* buf, unsigned int length);

  SocketFd            m_fd;

private:
  // Disable copying
  SocketBase(const SocketBase&);
  void operator = (const SocketBase&);
};

}

#endif // LIBTORRENT_SOCKET_BASE_H
