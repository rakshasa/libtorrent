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

#ifndef LIBTORRENT_SOCKET_BASE_H
#define LIBTORRENT_SOCKET_BASE_H

#include <list>

struct sockaddr_in;

namespace torrent {

class SocketBase {
public:
  typedef std::list<SocketBase*> Sockets;

  SocketBase(int fd = -1) :
    m_fd(fd),
    m_readItr(m_readSockets.end()),
    m_writeItr(m_writeSockets.end()),
    m_exceptItr(m_exceptSockets.end()) {}

  virtual ~SocketBase();

  bool                in_read()           { return m_readItr != m_readSockets.end(); }
  bool                in_write()          { return m_writeItr != m_writeSockets.end(); }
  bool                in_except()         { return m_exceptItr != m_exceptSockets.end(); }

  static void         set_socket_nonblock(int fd);
  static void         set_socket_min_cost(int fd);
  static int          get_socket_error(int fd);

  static Sockets&     read_sockets()      { return m_readSockets; }
  static Sockets&     write_sockets()     { return m_writeSockets; }
  static Sockets&     except_sockets()    { return m_exceptSockets; }

  void                insert_read();
  void                insert_write();
  void                insert_except();

  void                remove_read();
  void                remove_write();
  void                remove_except();

  int                 fd()                { return m_fd; }

  virtual void        read() = 0;
  virtual void        write() = 0;
  virtual void        except() = 0;

  static void         make_sockaddr(const std::string& host, int port, sockaddr_in& sa);
  static int          make_socket(sockaddr_in& sa);

  static void         close_socket(int fd);

protected:
  bool read_buf(void* buf, unsigned int length, unsigned int& pos);
  bool write_buf(const void* buf, unsigned int length, unsigned int& pos);

  int                 m_fd;

private:
  // Disable copying
  SocketBase(const SocketBase&);
  void operator = (const SocketBase&);

  Sockets::iterator   m_readItr;
  Sockets::iterator   m_writeItr;
  Sockets::iterator   m_exceptItr;

  static Sockets      m_readSockets;
  static Sockets      m_writeSockets;
  static Sockets      m_exceptSockets;
};

}

#endif // LIBTORRENT_SOCKET_BASE_H
