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

#ifndef LIBTORRENT_BLOCK_LIST_H
#define LIBTORRENT_BLOCK_LIST_H

#include <vector>
#include <torrent/common.h>
#include <torrent/data/block.h>
#include <torrent/data/piece.h>

namespace torrent {

// Temporary workaround until we can use C++11's std::vector::emblace_back.
template <typename Type>
class no_copy_vector {
public:
  typedef Type        value_type;
  typedef size_t      size_type;
  typedef value_type& reference;
  typedef ptrdiff_t   difference_type;

  typedef value_type*       iterator;
  typedef const value_type* const_iterator;

  no_copy_vector() : m_size(0), m_values(NULL) {}
  ~no_copy_vector() { clear(); }

  size_type size() const { return m_size; }
  bool      empty() const { return m_size == 0; }

  void resize(size_type s) { clear(); m_size = s; m_values = new value_type[s]; }

  void clear() { delete [] m_values; m_values = NULL; m_size = 0; }

  iterator       begin() { return m_values; }
  const_iterator begin() const { return m_values; }

  iterator       end() { return m_values + m_size; }
  const_iterator end() const { return m_values + m_size; }

  value_type&    back() { return *(m_values + m_size - 1); }

  value_type&    operator[](size_type idx) { return m_values[idx]; }

private:
  no_copy_vector(const no_copy_vector&);
  void operator = (const no_copy_vector&);

  size_type m_size;
  Block*    m_values;
};

class LIBTORRENT_EXPORT BlockList : public no_copy_vector<Block> {
public:
  typedef no_copy_vector<Block> base_type;
  typedef uint32_t              size_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  // using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  // using base_type::rbegin;
  // using base_type::rend;

  using base_type::operator[];

  BlockList(const Piece& piece, uint32_t blockLength);
  ~BlockList();

  bool                is_all_finished() const       { return m_finished == size(); }

  const Piece&        piece() const                 { return m_piece; }

  uint32_t            index() const                 { return m_piece.index(); }

  priority_t          priority() const              { return m_priority; }
  void                set_priority(priority_t p)    { m_priority = p; }

  size_type           finished() const              { return m_finished; }
  void                inc_finished()                { m_finished++; }
  void                clear_finished()              { m_finished = 0; }
  
  uint32_t            failed() const                { return m_failed; }

  // Temporary, just increment for now.
  void                inc_failed()                  { m_failed++; }

  uint32_t            attempt() const               { return m_attempt; }
  void                set_attempt(uint32_t a)       { m_attempt = a; }

  // Set when the chunk was initially requested from a seeder. This
  // allows us to quickly determine if it is a suitable chunk to
  // request from another seeder, e.g by already knowing it is a rare
  // piece.
  bool                by_seeder() const             { return m_bySeeder; }
  void                set_by_seeder(bool state)     { m_bySeeder = state; }

  void                do_all_failed();

private:
  BlockList(const BlockList&);
  void operator = (const BlockList&);

  Piece               m_piece;
  priority_t          m_priority;

  size_type           m_finished;
  uint32_t            m_failed;
  uint32_t            m_attempt;

  bool                m_bySeeder;
};

}

#endif
