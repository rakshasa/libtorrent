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

#ifndef LIBTORRENT_NET_PROTOCOL_BASE_H
#define LIBTORRENT_NET_PROTOCOL_BASE_H

#include "net/protocol_buffer.h"

namespace torrent {

class Piece;

class ProtocolBase {
public:
  typedef ProtocolBuffer<512> Buffer;
  typedef uint32_t            size_type;

  static const size_type buffer_size = 512;

  typedef enum {
    CHOKE = 0,
    UNCHOKE,
    INTERESTED,
    NOT_INTERESTED,
    HAVE,
    BITFIELD,
    REQUEST,
    PIECE,
    CANCEL,
    NONE,           // These are not part of the protocol
    KEEP_ALIVE      // Last command was a keep alive
  } Protocol;

  typedef enum {
    IDLE,
    MSG,
    READ_PIECE,
    READ_SKIP_PIECE,
    WRITE_PIECE,
    INTERNAL_ERROR
  } State;

  ProtocolBase() :
    m_choked(true),
    m_interested(false),
    m_state(IDLE),
    m_lastCommand(NONE) {

    m_buffer.reset();
  }

  bool                choked() const                          { return m_choked; }
  void                set_choked(bool s)                      { m_choked = s; }

  bool                interested() const                      { return m_interested; }
  void                set_interested(bool s)                  { m_interested = s; }

  Protocol            last_command() const                    { return m_lastCommand; }
  void                set_last_command(Protocol p)            { m_lastCommand = p; }

  Buffer*             buffer()                                { return &m_buffer; }

  State               get_state() const                       { return m_state; }
  void                set_state(State s)                      { m_state = s; }

  Piece               read_request();
  Piece               read_piece(size_type length);

  void                write_command(Protocol c)               { m_buffer.write_8(m_lastCommand = c); }

  void                write_keepalive();
  void                write_choke(bool s);
  void                write_interested(bool s);
  void                write_have(uint32_t index);
  void                write_bitfield(size_type length);
  void                write_request(const Piece& p);
  void                write_cancel(const Piece& p);
  void                write_piece(const Piece& p);

  static const size_type sizeof_keepalive    = 4;
  static const size_type sizeof_choke        = 5;
  static const size_type sizeof_interested   = 5;
  static const size_type sizeof_have         = 9;
  static const size_type sizeof_have_body    = 4;
  static const size_type sizeof_bitfield     = 5;
  static const size_type sizeof_request      = 17;
  static const size_type sizeof_request_body = 12;
  static const size_type sizeof_cancel       = 17;
  static const size_type sizeof_cancel_body  = 12;
  static const size_type sizeof_piece        = 13;
  static const size_type sizeof_piece_body   = 8;

  bool                can_write_keepalive() const             { return m_buffer.reserved_left() >= sizeof_keepalive; }
  bool                can_write_choke() const                 { return m_buffer.reserved_left() >= sizeof_choke; }
  bool                can_write_interested() const            { return m_buffer.reserved_left() >= sizeof_interested; }
  bool                can_write_have() const                  { return m_buffer.reserved_left() >= sizeof_have; }
  bool                can_write_bitfield() const              { return m_buffer.reserved_left() >= sizeof_bitfield; }
  bool                can_write_request() const               { return m_buffer.reserved_left() >= sizeof_request; }
  bool                can_write_cancel() const                { return m_buffer.reserved_left() >= sizeof_cancel; }
  bool                can_write_piece() const                 { return m_buffer.reserved_left() >= sizeof_piece; }

  bool                can_read_have_body() const              { return m_buffer.remaining() >= sizeof_have_body; }
  bool                can_read_request_body() const           { return m_buffer.remaining() >= sizeof_request_body; }
  bool                can_read_cancel_body() const            { return m_buffer.remaining() >= sizeof_request_body; }
  bool                can_read_piece_body() const             { return m_buffer.remaining() >= sizeof_piece_body; }

protected:
  bool                m_choked;
  bool                m_interested;

  State               m_state;
  Protocol            m_lastCommand;

  Buffer              m_buffer;
};

inline Piece
ProtocolBase::read_request() {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();
  uint32_t length = m_buffer.read_32();
  
  return Piece(index, offset, length);
}

inline Piece
ProtocolBase::read_piece(size_type length) {
  uint32_t index = m_buffer.read_32();
  uint32_t offset = m_buffer.read_32();

  return Piece(index, offset, length);
}

inline void
ProtocolBase::write_keepalive() {
  m_buffer.write_32(0);
  m_lastCommand = KEEP_ALIVE;
}

inline void
ProtocolBase::write_choke(bool s) {
  m_buffer.write_32(1);
  write_command(s ? CHOKE : UNCHOKE);
}

inline void
ProtocolBase::write_interested(bool s) {
  m_buffer.write_32(1);
  write_command(s ? INTERESTED : NOT_INTERESTED);
}

inline void
ProtocolBase::write_have(uint32_t index) {
  m_buffer.write_32(5);
  write_command(HAVE);
  m_buffer.write_32(index);
}

inline void
ProtocolBase::write_bitfield(size_type length) {
  m_buffer.write_32(1 + length);
  write_command(BITFIELD);
}

inline void
ProtocolBase::write_request(const Piece& p) {
  m_buffer.write_32(13);
  write_command(REQUEST);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
  m_buffer.write_32(p.length());
}

inline void
ProtocolBase::write_cancel(const Piece& p) {
  m_buffer.write_32(13);
  write_command(CANCEL);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
  m_buffer.write_32(p.length());
}

inline void
ProtocolBase::write_piece(const Piece& p) {
  m_buffer.write_32(9 + p.length());
  write_command(PIECE);
  m_buffer.write_32(p.index());
  m_buffer.write_32(p.offset());
}

}

#endif
