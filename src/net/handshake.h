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

#ifndef LIBTORRENT_HANDSHAKE_H
#define LIBTORRENT_HANDSHAKE_H

#include <inttypes.h>

#include "socket_base.h"
#include "peer_info.h"

namespace torrent {

class HandshakeManager;

class Handshake : public SocketBase {
public:
  Handshake(SocketFd fd, HandshakeManager* m) :
    SocketBase(fd),
    m_manager(m),
    m_buf(new char[256 + 48]),
    m_pos(0) {}

  virtual ~Handshake();

  const PeerInfo&     get_peer() { return m_peer; }
  const std::string&  get_hash() { return m_hash; }
  const std::string&  get_id()   { return m_id; }

  void                set_manager(HandshakeManager* m) { m_manager = m; }
  void                set_fd(SocketFd fd)              { m_fd = fd; }

  void                close();

protected:
  
  // Make sure you don't touch anything in the class after calling these,
  // return immidiately.
  void send_connected();
  void send_failed();

  bool recv1();
  bool recv2();

  PeerInfo          m_peer;
  std::string       m_hash;
  std::string       m_id;

  HandshakeManager* m_manager;

  char*             m_buf;
  uint32_t          m_pos;
};

}

#endif

