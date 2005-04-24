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

#ifndef LIBTORRENT_NET_PROTOCOL_BUFFER_H
#define LIBTORRENT_NET_PROTOCOL_BUFFER_H

#include <inttypes.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"

namespace torrent {

template <uint16_t tmpl_size>
class ProtocolBuffer {
public:
  typedef uint8_t     value_type;
  typedef value_type* iterator;
  typedef uint16_t    size_type;
  typedef size_type   difference_type;

  void                reset_position()                        { m_position = m_buffer; }
  void                move_position(size_type v)              { m_position += v; }

  void                set_end(size_type v)                    { m_end = m_buffer + v; }

  uint8_t             read8();
  uint8_t             peek8();
  uint32_t            read32();
  uint32_t            peek32();

  void                write8(uint8_t v);
  void                write32(uint32_t v);

  iterator            begin()                                 { return m_buffer; }
  iterator            position()                              { return m_position; }
  iterator            end()                                   { return m_end; }

  size_type           size_position() const                   { return m_position - m_buffer; }
  size_type           size_end() const                        { return m_end - m_buffer; }
  size_type           remaining() const                       { return m_end - m_position; }
  size_type           reserved() const                        { return tmpl_size; }
  difference_type     reserved_left() const                   { return reserved() - size_position(); }

private:
  iterator            m_position;
  iterator            m_end;
  value_type          m_buffer[tmpl_size];
};

template <uint16_t tmpl_size>
inline uint8_t
ProtocolBuffer<tmpl_size>::read8() {
  return *(m_position++);
}

template <uint16_t tmpl_size>
inline uint8_t
ProtocolBuffer<tmpl_size>::peek8() {
  return *m_position;
}

template <uint16_t tmpl_size>
inline uint32_t
ProtocolBuffer<tmpl_size>::read32() {
  uint32_t t = ntohl(*(uint32_t*)m_position);
  m_position += sizeof(uint32_t);

  return t;
}

template <uint16_t tmpl_size>
inline uint32_t
ProtocolBuffer<tmpl_size>::peek32() {
  return ntohl(*(uint32_t*)m_position);
}

template <uint16_t tmpl_size>
inline void
ProtocolBuffer<tmpl_size>::write8(uint8_t v) {
  *(m_position++) = v;

  if (m_position > m_buffer + tmpl_size)
    throw internal_error("ProtocolBuffer tried to write beyond scope of the buffer");
}

template <uint16_t tmpl_size>
inline void
ProtocolBuffer<tmpl_size>::write32(uint32_t v) {
  *(uint32_t*)m_position = htonl(v);
  m_position += sizeof(uint32_t);

  if (m_position > m_buffer + tmpl_size)
    throw internal_error("ProtocolBuffer tried to write beyond scope of the buffer");
}

}

#endif
