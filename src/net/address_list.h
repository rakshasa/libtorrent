// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H
#define LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H

#include <list>
#include <string>
#include <rak/socket_address.h>

#include <torrent/object.h>

namespace torrent {

class AddressList : public std::list<rak::socket_address> {
public:
  // Parse normal or compact list of addresses and add to AddressList
  void                        parse_address_normal(const Object::list_type& b);
  void                        parse_address_compact(const std::string& s);

private:
  static rak::socket_address  parse_address(const Object& b);

  struct add_address : public std::unary_function<rak::socket_address, void> {
    add_address(AddressList* l) : m_list(l) {}

    void operator () (const rak::socket_address& sa) const {
      if (!sa.is_valid())
        return;
 
      m_list->push_back(sa);
    }

    AddressList* m_list;
  };

};

}

#endif
