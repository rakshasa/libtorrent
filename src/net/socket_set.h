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

#ifndef LIBTORRENT_NET_SOCKET_SET_H
#define LIBTORRENT_NET_SOCKET_SET_H

#include <vector>
#include <list>

#include "torrent/exceptions.h"
#include "socket_base.h"

namespace torrent {

// Note that this class assume we don't overflow SocketSet::Index,
// which is in this case 2^15.

class SocketSet : private std::vector<SocketBase*> {
public:

  typedef int32_t                  Index;
  typedef std::vector<SocketBase*> Base;
  typedef std::vector<Index>       Table;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::empty;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  bool                has(SocketBase* s) const               { return _index(s) >= 0; }

  iterator            find(SocketBase* s);
  void                insert(SocketBase* s);
  void                erase(SocketBase* s);

  // Remove all erased elements from the container.
  void                prepare();

  void                reserve(size_t base, size_t openMax);

private:
  Index&              _index(SocketBase* s)                  { return m_table[s->get_fd().get_fd()]; }
  const Index&        _index(SocketBase* s) const            { return m_table[s->get_fd().get_fd()]; }

  inline void         _replace_with_last(Index idx);

  // TODO: Table of indexes or iterators?
  Table               m_table;
  Table               m_erased;
};

inline SocketSet::iterator
SocketSet::find(SocketBase* s) {
  if (_index(s) < 0)
    return end();

  return begin() + _index(s);
}

inline void
SocketSet::insert(SocketBase* s) {
  if (s->get_fd().get_fd() < 0)
    throw internal_error("Tried to insert a negative file descriptor from SocketSet");

  if (_index(s) >= 0)
    return;
  
  _index(s) = size();
  Base::push_back(s);
}

inline void
SocketSet::erase(SocketBase* s) {
  if (s->get_fd().get_fd() < 0)
    throw internal_error("Tried to erase a negative file descriptor from SocketSet");

  Index idx = _index(s);

  if (idx < 0)
    return;

  _index(s) = -1;

  *(begin() + idx) = NULL;
  m_erased.push_back(idx);
}

inline void
SocketSet::reserve(size_t base, size_t openMax) {
  Base::reserve(base);
  m_table.resize(openMax, -1);
}

}

#endif
