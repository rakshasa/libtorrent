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

#ifndef LIBTORRENT_PIECE_H
#define LIBTORRENT_PIECE_H

#include <inttypes.h>

namespace torrent {

class Piece {
public:
  Piece() : m_index(-1), m_offset(0), m_length(0) {}

  Piece(int32_t index, uint32_t offset, uint32_t length) :
    m_index(index), m_offset(offset), m_length(length) {}

  int32_t   get_index() const             { return m_index; }
  uint32_t  get_offset() const            { return m_offset; }
  uint32_t  get_length() const            { return m_length; }

  void      set_index(int32_t v)          { m_index = v; }
  void      set_offset(uint32_t v)        { m_offset = v; }
  void      set_length(uint32_t v)        { m_length = v; }

  bool operator == (const Piece& p) const { return m_index == p.m_index && m_offset == p.m_offset && m_length == p.m_length; }
  bool operator != (const Piece& p) const { return !(*this == p); }

private:
  int32_t   m_index;
  uint32_t  m_offset;
  uint32_t  m_length;
};

}

#endif // LIBTORRENT_PIECE_H

