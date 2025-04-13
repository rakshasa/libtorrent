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

#ifndef LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H
#define LIBTORRENT_DOWNLOAD_AVAILABLE_LIST_H

#include <vector>
#include <list>

#include <rak/socket_address.h>

#include "net/address_list.h"

namespace torrent {

class AvailableList : private std::vector<rak::socket_address> {
public:
  using base_type = std::vector<rak::socket_address>;
  using size_type = uint32_t;

  using base_type::value_type;
  using base_type::reference;
  using base_type::const_reference;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::capacity;
  using base_type::reserve;
  using base_type::empty;
  using base_type::clear;

  using base_type::back;
  using base_type::pop_back;
  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  value_type          pop_random();

  // Fuzzy size limit.
  size_type           max_size() const                   { return m_maxSize; }
  void                set_max_size(size_type s)          { m_maxSize = s; }

  bool                want_more() const                  { return size() <= m_maxSize; }

  // This push is somewhat inefficient as it iterates through the
  // whole container to see if the address already exists.
  void                push_back(const rak::socket_address* sa);

  void                insert(AddressList* l);
  void                erase(const rak::socket_address& sa);
  void                erase(iterator itr)                 { *itr = back(); pop_back(); }
  
  // A place to temporarily put addresses before re-adding them to the
  // AvailableList.
  AddressList*        buffer()                            { return &m_buffer; }

private:
  size_type           m_maxSize{1000};

  AddressList         m_buffer;
};

}

#endif
