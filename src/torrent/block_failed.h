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

#ifndef LIBTORRENT_BLOCK_FAILED_H
#define LIBTORRENT_BLOCK_FAILED_H

#include <algorithm>
#include <functional>
#include <vector>
#include <inttypes.h>
#include <torrent/common.h>

namespace torrent {

class BlockFailed : public std::vector<std::pair<char*, uint32_t> > {
public:
  typedef std::vector<std::pair<char*, uint32_t> > base_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::size_type;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::operator[];

  static const uint32_t invalid_index = ~uint32_t();

  BlockFailed() : m_current(invalid_index) {}
  ~BlockFailed();

  size_type           current() const                   { return m_current; }
  iterator            current_iterator()                { return begin() + m_current; }
  reverse_iterator    current_reverse_iterator()        { return reverse_iterator(begin() + m_current + 1); }

  void                set_current(size_type idx)        { m_current = idx; }
  void                set_current(iterator itr)         { m_current = itr - begin(); }
  void                set_current(reverse_iterator itr) { m_current = itr.base() - begin() - 1; }

  iterator            max_element();
  reverse_iterator    reverse_max_element();

private:
  BlockFailed(const BlockFailed&);
  void operator = (const BlockFailed&);

  static void         delete_entry(const reference e)                         { delete e.first; }
  static bool         compare_entries(const reference e1, const reference e2) { return e1.second < e2.second; }

  size_type           m_current;
};

inline
BlockFailed::~BlockFailed() {
  std::for_each(begin(), end(), std::ptr_fun(&BlockFailed::delete_entry));
}

inline BlockFailed::iterator
BlockFailed::max_element() {
  return std::max_element(begin(), end(), std::ptr_fun(&BlockFailed::compare_entries));
}

inline BlockFailed::reverse_iterator
BlockFailed::reverse_max_element() {
  return std::max_element(rbegin(), rend(), std::ptr_fun(&BlockFailed::compare_entries));
}

}

#endif
