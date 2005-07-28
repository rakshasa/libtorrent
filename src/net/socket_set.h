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

#ifndef LIBTORRENT_NET_SOCKET_SET_H
#define LIBTORRENT_NET_SOCKET_SET_H

#include <list>
#include <vector>
#include <inttypes.h>

#include "torrent/exceptions.h"
#include "torrent/event.h"

namespace torrent {

// SocketSet's Base is a vector of active SocketBase
// instances. 'm_table' is a vector with the size 'openMax', each
// element of which points to an active instance in the Base vector.

// Propably should rename to EventSet...

class SocketSet : private std::vector<Event*> {
public:
  typedef uint32_t               size_type;
  typedef std::vector<Event*>    Base;
  typedef std::vector<size_type> Table;

  static const size_type npos = static_cast<size_type>(-1);

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::empty;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  bool                has(Event* s) const                    { return _index(s) != npos; }

  iterator            find(Event* s);
  void                insert(Event* s);
  void                erase(Event* s);

  // Remove all erased elements from the container.
  void                prepare();
  // Allocate storage for fd's with up to 'openMax' value. TODO: Remove reserve
  void                reserve(size_t openMax)                { m_table.resize(openMax, npos); Base::reserve(openMax); }

private:
  size_type&          _index(Event* s)                       { return m_table[s->get_file_desc()]; }
  const size_type&    _index(Event* s) const                 { return m_table[s->get_file_desc()]; }

  inline void         _replace_with_last(size_type idx);

  // TODO: Table of indexes or iterators?
  Table               m_table;
  Table               m_erased;
};

inline SocketSet::iterator
SocketSet::find(Event* s) {
  if (_index(s) == npos)
    return end();

  return begin() + _index(s);
}

inline void
SocketSet::insert(Event* s) {
  if (static_cast<size_type>(s->get_file_desc()) >= m_table.size())
    throw internal_error("Tried to insert an out-of-bounds file descriptor to SocketSet");

  if (_index(s) != npos)
    return;

  _index(s) = size();
  Base::push_back(s);
}

inline void
SocketSet::erase(Event* s) {
  if (static_cast<size_type>(s->get_file_desc()) >= m_table.size())
    throw internal_error("Tried to erase an out-of-bounds file descriptor from SocketSet");

  size_type idx = _index(s);

  if (idx == npos)
    return;

  _index(s) = npos;

  *(begin() + idx) = NULL;
  m_erased.push_back(idx);
}

}

#endif
