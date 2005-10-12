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

#ifndef LIBTORRENT_UTILS_BITFIELD_EXT_H
#define LIBTORRENT_UTILS_BITFIELD_EXT_H

#include "torrent/exceptions.h"
#include "bitfield.h"

// Bitfield with some extra information like bit count.

namespace torrent {

class BitFieldExt : private BitField {
public:
  using BitField::size_t;
  using BitField::size_bits;
  using BitField::size_bytes;
  using BitField::get;
  using BitField::begin;
  using BitField::end;
  using BitField::operator[];

  BitFieldExt() : m_count(0) {}
  BitFieldExt(size_t s) : BitField(s), m_count(0) {}

  BitFieldExt(const BitFieldExt& bf) : BitField(bf), m_count(bf.m_count) {}

  // Call this after modifying through the begin() pointer.
  void      update_count() {
    if (this == NULL)
      throw internal_error("BitFieldExt::update_count() called on a NULL object");

    m_count = BitField::count();
  }

  size_t    count() const    { return m_count; }
  bool      all_zero() const { return m_count == 0; }
  bool      all_set() const  { return m_count == size_bits(); }

  void      clear() {
    m_count = 0;
    BitField::clear();
  }

  void      set(size_t i, bool s) {
    if (get(i) == s)
      return;

    BitField::set(i, s);

    if (s)
      m_count++;
    else
      m_count--;
  }

  BitFieldExt&        operator = (const BitFieldExt& bf) {
    BitField::operator=(bf);
    return *this;
  }

  const BitField&     bitfield() { return *this; }

private:
  size_t m_count;
};

}

#endif
