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

#ifndef LIBTORRENT_NET_PROTOCOL_BASE_H
#define LIBTORRENT_NET_PROTOCOL_BASE_H

#include "data/storage.h"
#include "protocol_buffer.h"

namespace torrent {

class Piece;

class ProtocolBase {
public:
  typedef ProtocolBuffer<512>           Buffer;
  typedef StorageChunk::iterator        ChunkPart;

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

  ProtocolBase() :
    m_position(0),
    m_choked(true),
    m_interested(false),
    m_lastCommand(NONE) {}

  bool                get_choked() const                      { return m_choked; }
  bool                get_interested() const                  { return m_interested; }

  void                set_choked(bool s)                      { m_choked = s; }
  void                set_interested(bool s)                  { m_interested = s; }

  Protocol            get_last_command() const                { return m_lastCommand; }
  void                set_last_command(Protocol p)            { m_lastCommand = p; }

  Buffer&             get_buffer()                            { return m_buffer; }

  // Position should perhaps be in a different place, like a dedicated
  // chunk writing class.
  uint32_t            get_position() const                    { return m_position; }
  void                set_position(uint32_t p)                { m_position = p; }
  void                adjust_position(uint32_t p)             { m_position += p; }

protected:
  uint32_t            m_position;

  bool                m_choked;
  bool                m_interested;

  Protocol            m_lastCommand;
  Buffer              m_buffer;
};

}

#endif
