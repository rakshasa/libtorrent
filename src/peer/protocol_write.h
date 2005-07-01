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

#ifndef LIBTORRENT_NET_PROTOCOL_WRITE_H
#define LIBTORRENT_NET_PROTOCOL_WRITE_H

#include "data/piece.h"

#include "protocol_base.h"

namespace torrent {

class ProtocolWrite : public ProtocolBase {
public:
  typedef enum {
    IDLE,
    MSG,
    WRITE_BITFIELD,
    WRITE_PIECE,
    SHUTDOWN
  } State;

  ProtocolWrite() : m_state(IDLE) {}

  State               get_state() const                       { return m_state; }
  void                set_state(State s)                      { m_state = s; }

  void                write_command(Protocol c)               { m_buffer.write8(m_lastCommand = c); }

  void                write_keepalive();
  void                write_choke(bool s);
  void                write_interested(bool s);
  void                write_have(uint32_t index);
  void                write_bitfield(uint32_t length);
  void                write_request(const Piece& p, bool s = true);
  void                write_piece(const Piece& p);

  bool                can_write_keepalive() const             { return m_buffer.reserved_left() >= 4; }
  bool                can_write_choke() const                 { return m_buffer.reserved_left() >= 5; }
  bool                can_write_interested() const            { return m_buffer.reserved_left() >= 5; }
  bool                can_write_have() const                  { return m_buffer.reserved_left() >= 9; }
  bool                can_write_bitfield() const              { return m_buffer.reserved_left() >= 5; }
  bool                can_write_request() const               { return m_buffer.reserved_left() >= 17; }
  bool                can_write_piece() const                 { return m_buffer.reserved_left() >= 13; }

private:
  State               m_state;
};

inline void
ProtocolWrite::write_keepalive() {
  m_buffer.write32(0);
  m_lastCommand = KEEP_ALIVE;
}

inline void
ProtocolWrite::write_choke(bool s) {
  m_buffer.write32(1);
  write_command(s ? CHOKE : UNCHOKE);
}

inline void
ProtocolWrite::write_interested(bool s) {
  m_buffer.write32(1);
  write_command(s ? INTERESTED : NOT_INTERESTED);
}

inline void
ProtocolWrite::write_have(uint32_t index) {
  m_buffer.write32(5);
  write_command(HAVE);
  m_buffer.write32(index);
}

inline void
ProtocolWrite::write_bitfield(uint32_t length) {
  m_buffer.write32(1 + length);
  write_command(BITFIELD);
}

inline void
ProtocolWrite::write_request(const Piece& p, bool s) {
  m_buffer.write32(13);
  write_command(s ? REQUEST : CANCEL);
  m_buffer.write32(p.get_index());
  m_buffer.write32(p.get_offset());
  m_buffer.write32(p.get_length());
}

inline void
ProtocolWrite::write_piece(const Piece& p) {
  m_buffer.write32(9 + p.get_length());
  write_command(PIECE);
  m_buffer.write32(p.get_index());
  m_buffer.write32(p.get_offset());
}

}

#endif
