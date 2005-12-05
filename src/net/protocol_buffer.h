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

  void                reset()                       { m_position = m_end = begin(); }
  void                reset_position()              { m_position = m_buffer; }
  void                move_position(size_type v)    { m_position += v; }

  void                set_position_itr(iterator itr) { m_position = itr; }

  void                prepare_end()                 { m_end = m_position; reset_position(); }
  void                set_end(size_type v)          { m_end = m_buffer + v; }
  void                move_end(size_type v)         { m_end += v; }

  uint8_t             read_8()                      { return *m_position++; }
  uint8_t             peek_8()                      { return *m_position; }
  uint16_t            read_16();
  uint32_t            read_32();
  uint32_t            peek_32();
  uint64_t            read_64()                     { return read_int<uint64_t>(); }
  uint64_t            peek_64()                     { return peek_int<uint64_t>(); }

  template <typename Out>
  void                read_range(Out first, Out last);

  template <typename T>
  inline T            read_int();

  template <typename T>
  inline T            peek_int();

  void                write_8(uint8_t v)            { *m_position++ = v; validate_position(); }
  void                write_16(uint16_t v);
  void                write_32(uint32_t v);
  void                write_64(uint64_t v)          { write_int<uint64_t>(v); }

  template <typename T>
  inline void         write_int(T t);

  template <typename In>
  void                write_range(In first, In last);

  iterator            begin()                       { return m_buffer; }
  iterator            position()                    { return m_position; }
  iterator            end()                         { return m_end; }

  size_type           size_position() const         { return m_position - m_buffer; }
  size_type           size_end() const              { return m_end - m_buffer; }
  size_type           remaining() const             { return m_end - m_position; }
  size_type           reserved() const              { return tmpl_size; }
  difference_type     reserved_left() const         { return reserved() - size_position(); }

private:
  void                validate_position() const {
#ifdef USE_EXTRA_DEBUG
    if (m_position > m_buffer + tmpl_size)
      throw internal_error("ProtocolBuffer tried to write beyond scope of the buffer.");
#endif
  }

  iterator            m_position;
  iterator            m_end;
  value_type          m_buffer[tmpl_size];
};

template <uint16_t tmpl_size>
inline uint16_t
ProtocolBuffer<tmpl_size>::read_16() {
#ifndef USE_ALIGNED
  uint16_t t = ntohs(*reinterpret_cast<uint16_t*>(m_position));
  m_position += sizeof(uint16_t);

  return t;
#else
  return read_int<uint16_t>();
#endif
}

template <uint16_t tmpl_size>
inline uint32_t
ProtocolBuffer<tmpl_size>::read_32() {
#ifndef USE_ALIGNED
  uint32_t t = ntohl(*reinterpret_cast<uint32_t*>(m_position));
  m_position += sizeof(uint32_t);

  return t;
#else
  return read_int<uint32_t>();
#endif
}

template <uint16_t tmpl_size>
inline uint32_t
ProtocolBuffer<tmpl_size>::peek_32() {
#ifndef USE_ALIGNED
  return ntohl(*reinterpret_cast<uint32_t*>(m_position));
#else
  return peek_int<uint32_t>();
#endif
}

template <uint16_t tmpl_size>
template <typename T>
inline T
ProtocolBuffer<tmpl_size>::read_int() {
  T t = T();

  for (iterator last = m_position + sizeof(T); m_position != last; ++m_position)
    t = (t << 8) + *m_position;

  return t;
}

template <uint16_t tmpl_size>
template <typename T>
inline T
ProtocolBuffer<tmpl_size>::peek_int() {
  T t = T();

  for (iterator itr = m_position, last = m_position + sizeof(T); itr != last; ++itr)
    t = (t << 8) + *itr;

  return t;
}

template <uint16_t tmpl_size>
inline void
ProtocolBuffer<tmpl_size>::write_16(uint16_t v) {
#ifndef USE_ALIGNED
  *reinterpret_cast<uint16_t*>(m_position) = htons(v);
  m_position += sizeof(uint16_t);

  validate_position();
#else
  write_int<uint16_t>(v);
#endif
}

template <uint16_t tmpl_size>
inline void
ProtocolBuffer<tmpl_size>::write_32(uint32_t v) {
#ifndef USE_ALIGNED
  *reinterpret_cast<uint32_t*>(m_position) = htonl(v);
  m_position += sizeof(uint32_t);

  validate_position();
#else
  write_int<uint32_t>(v);
#endif
}

template <uint16_t tmpl_size>
template <typename Out>
void
ProtocolBuffer<tmpl_size>::read_range(Out first, Out last) {
  for ( ; first != last; ++m_position, ++first)
    *first = *m_position;

  validate_position();
}

template <uint16_t tmpl_size>
template <typename T>
inline void
ProtocolBuffer<tmpl_size>::write_int(T v) {
  for (iterator itr = m_position + sizeof(T); itr != m_position; v >>= 8)
    *(--itr) = v;

  m_position += sizeof(T);
  validate_position();
}

template <uint16_t tmpl_size>
template <typename In>
void
ProtocolBuffer<tmpl_size>::write_range(In first, In last) {
  for ( ; first != last; ++m_position, ++first)
    *m_position = *first;

  validate_position();
}

}

#endif
