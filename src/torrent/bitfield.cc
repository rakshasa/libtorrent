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

#include "config.h"

#include <algorithm>

#include "bitfield.h"
#include "exceptions.h"

namespace torrent {

// *sigh*
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

void
Bitfield::set_size_bits(size_type s) {
  if (m_data != NULL)
    throw internal_error("Bitfield::set_size_bits(size_type s) m_data != NULL.");

  m_size = s;
}

void
Bitfield::set_size_set(size_type s) {
  if (s > m_size)
    throw internal_error("Bitfield::set_size_set(size_type s) s > m_size.");

  m_set = s;
}

void
Bitfield::update() {
  // Clears the unused bits.
  clear_tail();

  m_set = 0;

  // Some archs have bitcounting instructions, look into writing a
  // wrapper for those.
  for (iterator itr = m_data, last = end(); itr != last; ++itr)
    m_set += bit_count_256[*itr];
}

void
Bitfield::copy(const Bitfield& bf) {
  m_size = bf.m_size;
  m_set = bf.m_set;

  if (bf.m_data == NULL) {
    m_data = NULL;

  } else {
    m_data = new value_type[size_bytes()];

    std::memcpy(m_data, bf.m_data, size_bytes());
  }
}

void
Bitfield::swap(Bitfield& bf) {
  std::swap(m_size, bf.m_size);
  std::swap(m_set, bf.m_set);
  std::swap(m_data, bf.m_data);
}

void
Bitfield::set_all() {
  m_set = m_size;

  std::memset(m_data, ~value_type(), size_bytes());
}

void
Bitfield::unset_all() {
  m_set = 0;

  std::memset(m_data, value_type(), size_bytes());
}

// Quick hack. Speed improvements would require that m_set is kept
// up-to-date.
void
Bitfield::set_range(size_type first, size_type last) {
  while (first != last)
    set(first++);
}

void
Bitfield::unset_range(size_type first, size_type last) {
  while (first != last)
    unset(first++);
}

}
