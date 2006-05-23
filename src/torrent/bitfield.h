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

#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>
#include <inttypes.h>

namespace torrent {

class Bitfield {
public:
  typedef uint32_t              size_type;
  typedef uint8_t               value_type;
  typedef const uint8_t         const_value_type;
  typedef value_type*           iterator;
  typedef const value_type*     const_iterator;

  Bitfield() : m_size(0), m_set(0), m_data(NULL)    {}
  ~Bitfield()                                       { clear(); }

  bool                empty() const                 { return m_data != NULL; }

  bool                is_all_set() const            { return m_set == m_size; }
  bool                is_all_unset() const          { return m_set == 0; }

  bool                is_tail_cleared() const       { return m_size % 8 == 0 || !((*(end() - 1) & mask_from(m_size % 8))); }

  size_type           size_bits() const             { return m_size; }
  size_type           size_bytes() const            { return (m_size + 7) / 8; }

  size_type           size_set() const              { return m_set; }
  size_type           size_unset() const            { return m_size - m_set; }

  void                set_size_bits(size_type s);
  void                set_size_set(size_type s);

  // Call update if you've changed the data directly and want to
  // update the counters and unset the last unused bits.
  //
  // Resize clears the data?
  void                update();

  void                allocate()                    { if (m_data == NULL) m_data = new value_type[size_bytes()]; }
  void                unallocate()                  { delete [] m_data; m_data = NULL; }

  void                clear()                       { unallocate(); m_size = 0; m_set = 0; }
  void                clear_tail()                  { if (m_size % 8) *(end() - 1) &= mask_before(m_size % 8); }

  void                copy(const Bitfield& bf);

  void                set_all();
  void                set_range(size_type first, size_type last);

  void                unset_all();
  void                unset_range(size_type first, size_type last);

  bool                get(size_type idx) const      { return m_data[idx / 8] & mask_at(idx % 8); }

  void                set(size_type idx)            { m_set += !get(idx); m_data[idx / 8] |=  mask_at(idx % 8); }
  void                unset(size_type idx)          { m_set -=  get(idx); m_data[idx / 8] &= ~mask_at(idx % 8); }

  iterator            begin()                       { return m_data; }
  const_iterator      begin() const                 { return m_data; }
  iterator            end()                         { return m_data + size_bytes(); }
  const_iterator      end() const                   { return m_data + size_bytes(); }

  size_type           position(const_iterator itr) const  { return (itr - m_data) * 8; }

  void                from_c_str(const char* str)   { std::memcpy(m_data, str, size_bytes()); update(); }

  // Remember to use modulo.
  static value_type   mask_at(size_type idx)        { return 1 << (7 - idx); }
  static value_type   mask_before(size_type idx)    { return (value_type)~0 << (8 - idx); }
  static value_type   mask_from(size_type idx)      { return (value_type)~0 >> idx; }

private:
  Bitfield(const Bitfield& bf);
  Bitfield& operator = (const Bitfield& bf);

  size_type           m_size;
  size_type           m_set;

  value_type*         m_data;
};

}

#endif
