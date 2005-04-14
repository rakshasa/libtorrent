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

#include "socket_fd.h"

namespace torrent {

// Note that this class assume we don't overflow SocketSet::Index,
// which is in this case 2^15.

class SocketSet : private std::vector<SocketFd> {
public:

  typedef int16_t               Index;
  typedef std::vector<SocketFd> Base;
  typedef std::vector<Index>    Table;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::empty;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  iterator            find(SocketFd fd);
  iterator            insert(SocketFd fd);
  iterator            erase(SocketFd fd);

  void                reserve(size_t sb, size_t st);

private:
  Index&              _index(SocketFd fd)           { return m_table[fd.get_fd()]; }
  const Index&        _index(SocketFd fd) const     { return m_table[fd.get_fd()]; }

  iterator            _replace_with_last(Index i);

  Table               m_table;
};

inline SocketSet::iterator
SocketSet::find(SocketFd fd) {
  if (_index(fd) < 0)
    return end();

  return begin() + _index(fd);
}

inline SocketSet::iterator
SocketSet::insert(SocketFd fd) {
  if (_index(fd) >= 0)
    return begin() + _index(fd);
  
  size_t s = _index(fd) = size();
  Base::push_back(fd);

  return begin() + s;
}

inline SocketSet::iterator
SocketSet::erase(SocketFd fd) {
  Index idx = _index(fd);

  if (idx < 0)
    return end();

  _index(fd) = -1;

  return _replace_with_last(idx);
}

inline SocketSet::iterator
SocketSet::_replace_with_last(Index i) {
  iterator itr = begin() + i;

  if (itr != end() - 1) {
    *itr = Base::back();
    _index(*itr) = i;
  }

  Base::pop_back();

  return itr;
}

void
SocketSet::reserve(size_t sb, size_t st) {
  Base::reserve(sb);
  m_table.reserve(st);
}

}

#endif
