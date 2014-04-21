// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#include "rak/algorithm.h"
#include "utils/instrumentation.h"

#include "bitfield.h"
#include "exceptions.h"

namespace torrent {

void
Bitfield::set_size_bits(size_type s) {
  if (m_data != NULL)
    throw internal_error("Bitfield::set_size_bits(size_type s) m_data != NULL.");

  m_size = s;
}

void
Bitfield::set_size_set(size_type s) {
  if (s > m_size || m_data != NULL)
    throw internal_error("Bitfield::set_size_set(size_type s) s > m_size.");

  m_set = s;
}

void
Bitfield::allocate() { 
  if (m_data != NULL)
    return;

  m_data = new value_type[size_bytes()];

  instrumentation_update(INSTRUMENTATION_MEMORY_BITFIELDS, (int64_t)size_bytes());
}

void
Bitfield::unallocate() {
  if (m_data == NULL)
    return;

  delete [] m_data;
  m_data = NULL;

  instrumentation_update(INSTRUMENTATION_MEMORY_BITFIELDS, -(int64_t)size_bytes());
}

void
Bitfield::update() {
  // Clears the unused bits.
  clear_tail();

  m_set = 0;

  iterator itr = m_data;
  iterator last = end();

  while (itr + sizeof(unsigned int) <= last) {
    m_set += rak::popcount_wrapper(*reinterpret_cast<unsigned int*>(itr));
    itr += sizeof(unsigned int);
  }

  while (itr != last) {
    m_set += rak::popcount_wrapper(*itr++);
  }
}

void
Bitfield::copy(const Bitfield& bf) {
  unallocate();

  m_size = bf.m_size;
  m_set = bf.m_set;

  if (bf.m_data == NULL) {
    m_data = NULL;
  } else {
    allocate();
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
  clear_tail();
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

// size_type
// Bitfield::count_range(size_type first, size_type last) {
//   size_type count = 0;

//   // Some archs have bitcounting instructions, look into writing a
//   // wrapper for those.
//   for (iterator itr = m_data, last = end(); itr != last; ++itr)
//     m_set += bit_count_256[*itr];
// }

}
