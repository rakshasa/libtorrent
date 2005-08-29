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

#ifndef LIBTORRENT_NET_PROTOCOL_READ_H
#define LIBTORRENT_NET_PROTOCOL_READ_H

#include "protocol_base.h"

namespace torrent {

class ProtocolRead : public ProtocolBase {
public:
  typedef enum {
    IDLE,
    LENGTH,
    TYPE,
    MSG,
    READ_BITFIELD,
    READ_PIECE,
    SKIP_PIECE,
    INTERNAL_ERROR
  } State;

  ProtocolRead() : m_state(IDLE), m_length(0) {}

  State               get_state() const             { return m_state; }
  void                set_state(State s)            { m_state = s; }

  uint32_t            get_length() const            { return m_length; }
  void                set_length(uint32_t l)        { m_length = l; }

  Piece               read_request();
  Piece               read_piece();

private:
  State               m_state;
  uint32_t            m_length;
};

inline Piece
ProtocolRead::read_request() {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();
  uint32_t length = m_buffer.read_32();
  
  return Piece(index, offset, length);
}

inline Piece
ProtocolRead::read_piece() {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();

  return Piece(index, offset, get_length() - 9);
}

}

#endif
