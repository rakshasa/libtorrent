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

#ifndef LIBTORRENT_NET_PROTOCOL_READ_H
#define LIBTORRENT_NET_PROTOCOL_READ_H

#include "protocol_buffer.h"

namespace torrent {

class ProtocolRead {
public:
  typedef ProtocolBuffer<512> Buffer;

  typedef enum {
    IDLE,
    LENGTH,
    TYPE,
    MSG,
    BITFIELD,
    PIECE,
    SKIP_PIECE
  } State;

  ProtocolRead() : m_state(IDLE) {}

  State               get_state() const             { return m_state; }
  void                set_state(State s)            { m_state = s; }

  Buffer&             get_buffer()                  { return m_buffer; }

private:
  State               m_state;
  Buffer              m_buffer;
};

}

#endif
