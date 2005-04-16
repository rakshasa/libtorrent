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

#ifndef LIBTORRENT_NET_POLL_H
#define LIBTORRENT_NET_POLL_H

#include "socket_set.h"

namespace torrent {

class Poll {
public:
  
  static void         set_open_max(int s);

  static SocketSet&   read_set() { return m_readSet; }
  static SocketSet&   write_set() { return m_writeSet; }
  static SocketSet&   except_set() { return m_exceptSet; }

private:
  static SocketSet    m_readSet;
  static SocketSet    m_writeSet;
  static SocketSet    m_exceptSet;
};

}

#endif
