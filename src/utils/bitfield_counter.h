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

#ifndef LIBTORRENT_BITFIELD_COUNTER_H
#define LIBTORRENT_BITFIELD_COUNTER_H

#include <vector>
#include "utils/bitfield.h"

namespace torrent {

class BitFieldCounter {
 public:
  // Use short since we're never going to get more than 2^16 peers connected
  // at once.
  typedef std::vector<uint16_t> Field;

  BitFieldCounter() : m_field(0) {}
  BitFieldCounter(int size) : m_field(size, 0) {}

  const Field& field() const { return m_field; }

  void      create(uint16_t size) { m_field = Field(size, 0); }

  uint16_t  inc(int index) { return ++m_field[index]; }
  uint16_t  dec(int index) { return --m_field[index]; }

  void      inc(const BitField& bf) {
    if (bf.size_bits() != m_field.size())
      throw internal_error("BitFieldCounter::inc called on fields with mismatching size");

    int a = 0;
    char* c = (char*)bf.begin();

    for (Field::iterator itr = m_field.begin(); itr != m_field.end(); ++itr, ++a) {
      if (a == 8) {
	a = 0;
	++c;
      }

      if (*c & (1 << (7 - a)))
	++*itr;
    }
  }

  void      dec(const BitField& bf) {
    if (bf.size_bits() != m_field.size())
      throw internal_error("BitFieldCounter::dec called on fields with mismatching size");

    int a = 0;
    char* c = (char*)bf.begin();

    for (Field::iterator itr = m_field.begin(); itr != m_field.end(); ++itr, ++a) {
      if (a == 8) {
	a = 0;
	++c;
      }

      if (*c & (1 << (7 - a)))
	--*itr;
    }
  }

 private:
  Field m_field;
};

}

#endif // LIBTORRENT_BITFIELD_COUNTER_H
