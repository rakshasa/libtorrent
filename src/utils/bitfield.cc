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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "bitfield.h"
#include "torrent/exceptions.h"

namespace torrent {
  
BitField::BitField(unsigned int s) :
  m_size(s) {

  if (s) {
    int a = (m_size + 7) / 8;

    m_start = new data_t[a];
    m_end = m_start + a;

  } else {
    m_start = m_end = NULL;
  }

  clear();
}

BitField::BitField(const BitField& bf) :
  m_size(bf.m_size) {

  if (m_size) {
    m_start = new data_t[bf.size_bytes()];
    m_end = m_start + bf.size_bytes();

    std::memcpy(m_start, bf.m_start, bf.size_bytes());

  } else {
    m_start = m_end = NULL;
  }
}

BitField&
BitField::operator = (const BitField& bf) {
  if (this == &bf)
    return *this;

  delete [] m_start;
  
  if (bf.m_size) {
    m_size = bf.m_size;

    m_start = new data_t[bf.size_bytes()];
    m_end = m_start + bf.size_bytes();
    
    std::memcpy(m_start, bf.m_start, bf.size_bytes());
    
  } else {
    m_size = 0;
    m_start = NULL;
    m_end = NULL;
  }
  
  return *this;
}

BitField&
BitField::not_in(const BitField& bf) {
  if (m_size != bf.m_size)
    throw internal_error("Tried to do operations between different sized bitfields");

  for (data_t* i = m_start, *i2 = bf.m_start; i < m_end; i++, i2++)
    *i &= ~*i2;

  return *this;
}

bool
BitField::all_zero() const {
  if (m_size == 0)
    return true;

  for (data_t* i = m_start; i < m_end; i++)
    if (*i)
      return false;

  return true;
}

bool
BitField::all_set() const {
  if (m_size == 0)
    return false;

  data_t* i = m_start;
  data_t* end = m_end;

  if (m_size % 8)
    --end;

  while (i != end) {
    if (*i != (data_t)~0)
      return false;

    ++i;
  }

  return m_size % 8 == 0 || *i == ~data_t() << (8 - m_size % 8);
}

static const unsigned char bit_count_256[] = 
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

uint32_t
BitField::count() const {
  // Might be faster since it doesn't touch the memory.
  // for (n = 0; x; n++)
  //   x &= x-1;

  size_t c = 0;

  for (data_t* i = m_start; i != m_end; ++i)
    c += bit_count_256[*i];

  return c;
}

void
BitField::set(size_t begin, size_t end, bool s) {
  // Quick, dirty and slow.
  while (begin != end)
    set(begin++, s);
}

}
