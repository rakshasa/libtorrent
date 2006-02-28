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

#ifndef LIBTORRENT_HANDSHAKE_H
#define LIBTORRENT_HANDSHAKE_H

#include <inttypes.h>

#include "net/socket_stream.h"
#include "protocol/peer_info.h"
#include "globals.h"

namespace torrent {

class HandshakeManager;
class TrackerInfo;

class Handshake : public SocketStream {
public:
  Handshake(SocketFd fd, HandshakeManager* m);
  virtual ~Handshake();

  const PeerInfo&     get_peer()                       { return m_peer; }
//   const std::string&  hash()                       { return m_hash; }
//   const std::string&  get_id()                         { return m_id; }
  TrackerInfo*        info()                           { return m_info; }

  void                set_manager(HandshakeManager* m) { m_manager = m; }

  void                clear_poll();
  void                close();

protected:
  Handshake(const Handshake&);
  void operator = (const Handshake&);
  
  // Make sure you don't touch anything in the class after calling these,
  // return immidiately.
  void                send_connected();
  void                send_failed();

  bool                recv1();
  bool                recv2();

  PeerInfo            m_peer;
//   std::string         m_hash;
//   std::string         m_id;
  TrackerInfo*        m_info;

  HandshakeManager*   m_manager;

  char*               m_buf;
  uint32_t            m_pos;

  rak::priority_item  m_taskTimeout;
};

}

#endif

