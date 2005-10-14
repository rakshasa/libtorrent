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

#ifndef LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H
#define LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H

#include <vector>
#include <list>

#include "net/socket_address.h"

namespace torrent {

class AvailableList : private std::vector<SocketAddress> {
public:
  typedef std::vector<SocketAddress> Base;
  typedef std::list<SocketAddress>   AddressList;
  typedef uint32_t                   size_type;

  using Base::value_type;
  using Base::reference;
  using Base::const_reference;

  using Base::iterator;
  using Base::const_iterator;
  using Base::reverse_iterator;
  using Base::size;
  using Base::empty;
  using Base::clear;

//   using Base::front;
  using Base::back;
//   using Base::pop_front;
  using Base::pop_back;
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  AvailableList() : m_maxSize(1000) {}

  value_type          pop_random();

  // Fuzzy size limit.
  size_type           get_max_size() const               { return m_maxSize; }
  void                set_max_size(size_type s)          { m_maxSize = s; }

  // This push is somewhat inefficient as it iterates through the
  // whole container to see if the address already exists.
  void                push_back(const SocketAddress& sa);

  void                insert(AddressList* l);
  void                erase(const SocketAddress& sa);
  void                erase(iterator itr)                 { *itr = back(); pop_back(); }

private:
  size_type           m_maxSize;
};

}

#endif
