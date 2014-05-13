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

#ifndef LIBTORRENT_UTILS_EXTENTS_H
#define LIBTORRENT_UTILS_EXTENTS_H

#include lt_tr1_array

#include <algorithm>

namespace torrent {

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
struct extents_base {
  typedef Key                          key_type;
  typedef std::pair<Key, Key>          range_type;
  typedef std::pair<extents_base*, Tp> mapped_type;
  typedef Tp                           mapped_value_type;

  typedef std::array<mapped_type, TableSize> table_type;
  
  extents_base(key_type pos, unsigned int mb, mapped_value_type val);
  extents_base(extents_base* parent, typename table_type::const_iterator itr);
  ~extents_base();

  bool         is_divisible(key_type key) const { return key % mask_bits == 0; }
  bool         is_leaf_branch() const           { return mask_bits == 0; }
  bool         is_equal_range(key_type first, key_type last, const mapped_value_type& val) const;

  unsigned int sizeof_data() const;

  typename table_type::iterator       partition_at(key_type key)           { return table.begin() + ((key >> mask_bits) & (TableSize - 1)); }
  typename table_type::const_iterator partition_at(key_type key) const     { return table.begin() + ((key >> mask_bits) & (TableSize - 1)); }

  unsigned int mask_distance(unsigned int mb) { return (~(~key_type() << mb) >> mask_bits); }

  key_type     partition_pos(typename table_type::const_iterator part) const { return position + (std::distance(table.begin(), part) << mask_bits); }

  void         insert(key_type pos, unsigned int mb, const mapped_value_type& val);

  const mapped_value_type& at(key_type key) const;

  unsigned int mask_bits;
  key_type     position;
  table_type   table;
};

template <typename Key, typename Tp, unsigned int MaskBits, unsigned int TableSize, unsigned int TableBits>
class extents : private extents_base<Key, Tp, TableSize, TableBits> {
public:
  typedef extents_base<Key, Tp, TableSize, TableBits> base_type;

  typedef typename base_type::key_type          key_type;
  typedef base_type                             value_type;
  typedef typename base_type::range_type        range_type;
  typedef typename base_type::mapped_type       mapped_type;
  typedef typename base_type::mapped_value_type mapped_value_type;
  typedef typename base_type::table_type        table_type;

  static const key_type mask_bits  = MaskBits;
  static const key_type table_bits = TableBits;
  static const key_type table_size = TableSize;

  using base_type::at;
  using base_type::sizeof_data;

  extents();

  bool is_equal_range(key_type first, key_type last, const mapped_value_type& val) const;

  void insert(key_type pos, unsigned int mb, const mapped_value_type& val);

  base_type* data() { return this; }
};

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
extents_base<Key, Tp, TableSize, TableBits>::extents_base(key_type pos, unsigned int mb, mapped_value_type val) :
  mask_bits(mb), position(pos) {
  std::fill(table.begin(), table.end(), mapped_type(NULL, mapped_value_type()));
}

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
extents_base<Key, Tp, TableSize, TableBits>::extents_base(extents_base* parent, typename table_type::const_iterator itr) :
  mask_bits(parent->mask_bits - TableBits), position(parent->partition_pos(itr)) {
  std::fill(table.begin(), table.end(), mapped_type(NULL, itr->second));
}

template <typename Key, typename Tp, unsigned int MaskBits, unsigned int TableSize, unsigned int TableBits>
extents<Key, Tp, MaskBits, TableSize, TableBits>::extents() :
  base_type(key_type(), mask_bits - table_bits, mapped_value_type())
{
}

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
extents_base<Key, Tp, TableSize, TableBits>::~extents_base() {
  for (typename table_type::const_iterator itr = table.begin(), last = table.end(); itr != last; itr++)
    delete itr->first;
}

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
unsigned int
extents_base<Key, Tp, TableSize, TableBits>::sizeof_data() const {
  unsigned int sum = sizeof(*this);

  for (typename table_type::const_iterator itr = table.begin(), last = table.end(); itr != last; itr++)
    if (itr->first != NULL)
      sum += itr->first->sizeof_data();

  return sum;
}

template <typename Key, typename Tp, unsigned int MaskBits, unsigned int TableSize, unsigned int TableBits>
void
extents<Key, Tp, MaskBits, TableSize, TableBits>::insert(key_type pos, unsigned int mb, const mapped_value_type& val) {
  key_type mask = ~key_type() << mb;

  base_type::insert(pos & mask, mb, val);
}

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
void
extents_base<Key, Tp, TableSize, TableBits>::insert(key_type pos, unsigned int mb, const mapped_value_type& val) {
  // RESTRICTED
  typename table_type::iterator first = partition_at(pos);
  typename table_type::iterator last = partition_at(pos) + mask_distance(mb) + 1;

  if (mb < mask_bits) {
    if (first->first == NULL)
      first->first = new extents_base(this, first);
    
    first->first->insert(pos, mb, val);
    return;
  }

  while (first != last) {
    if (first->first != NULL) {
      delete first->first;
      first->first = NULL;
    }
    
    (first++)->second = val;
  }
}

template <typename Key, typename Tp, unsigned int MaskBits, unsigned int TableSize, unsigned int TableBits>
bool
extents<Key, Tp, MaskBits, TableSize, TableBits>::is_equal_range(key_type first, key_type last, const mapped_value_type& val) const {
  // RESTRICTED
  first = std::max(first, key_type());
  last = std::min(last, key_type() + (~key_type() >> (sizeof(key_type) * 8 - MaskBits)));

  if (first <= last)
    return base_type::is_equal_range(first, last, val);
  else
    return true;
}

template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
bool
extents_base<Key, Tp, TableSize, TableBits>::is_equal_range(key_type key_first, key_type key_last, const mapped_value_type& val) const {
  // RESTRICTED
  typename table_type::const_iterator first = partition_at(key_first);
  typename table_type::const_iterator last = partition_at(key_last) + 1;

  do {
    //    std::cout << "shift_amount " << key_first << ' ' << key_last << std::endl;

    if (first->first == NULL && val != first->second)
      return false;

    if (first->first != NULL && !first->first->is_equal_range(std::max(key_first, partition_pos(first)),
                                                              std::min(key_last, partition_pos(first + 1) - 1), val))
      return false;

  } while (++first != last);

  return true;
}

// Assumes 'key' is within the range of the range.
template <typename Key, typename Tp, unsigned int TableSize, unsigned int TableBits>
const typename extents_base<Key, Tp, TableSize, TableBits>::mapped_value_type&
extents_base<Key, Tp, TableSize, TableBits>::at(key_type key) const {
  typename table_type::const_iterator itr = partition_at(key);

  while (itr->first != NULL)
    itr = itr->first->partition_at(key);

  return itr->second;
}

}

#endif
