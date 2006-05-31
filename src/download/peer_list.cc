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

#include "config.h"

#include <algorithm>
#include <functional>
#include <rak/functional.h>

#include "protocol/peer_info.h"
#include "torrent/exceptions.h"
#include "peer_list.h"

namespace torrent {

struct peer_list_equal_port : public std::binary_function<PeerList::reference, uint16_t, bool> {
  bool operator () (PeerList::reference p, uint16_t port) {
    return p.second->socket_address()->port() == port;
  }
};

void
PeerList::clear() {
  std::for_each(begin(), end(), rak::on(rak::mem_ptr_ref(&value_type::second), rak::call_delete<PeerInfo>()));

  base_type::clear();
}

PeerList::iterator
PeerList::connected(const rak::socket_address& sa) {
  range_type range = base_type::equal_range(sa);
  
  iterator itr = std::find_if(range.first, range.second, rak::bind2nd(peer_list_equal_port(), sa.port()));

  if (itr == range.second)
    itr = std::find_if(range.first, range.second, rak::on(rak::mem_ptr_ref(&value_type::second), std::not1(std::mem_fun(&PeerInfo::is_connected))));

  else if (itr->second->is_connected())
    return end();

  if (itr == range.second) {
    // Create a new entry.

    if (std::distance(range.first, range.second) >= 5)
      return end();

    itr = base_type::insert(range.second, value_type(rak::socket_address_key(sa), new PeerInfo()));

    itr->second->set_socket_address(&sa);

  } else {
    // Use an old entry.
    itr->second->socket_address()->set_port(sa.port());
  }

  itr->second->set_connected(true);

  return itr;
}

PeerList::iterator
PeerList::disconnected(iterator itr) {
  if (!itr->second->is_connected())
    throw internal_error("PeerList::disconnected(...) !itr->is_connected().");

  itr->second->set_connected(false);

  // Do magic to get rid of unneeded entries.
  return ++itr;
}

}
