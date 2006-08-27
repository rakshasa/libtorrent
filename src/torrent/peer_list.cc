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
#include <rak/socket_address.h>

#include "download/available_list.h"

#include "exceptions.h"
#include "globals.h"
#include "peer_info.h"
#include "peer_list.h"

namespace torrent {

bool
socket_address_key::operator < (const socket_address_key& sa) const {
  const rak::socket_address* sa1 = rak::socket_address::cast_from(m_sockaddr);
  const rak::socket_address* sa2 = rak::socket_address::cast_from(sa.m_sockaddr);

  if (sa1->family() != sa2->family())
    return sa1->family() > sa2->family();

  else if (sa1->family() == rak::socket_address::af_inet)
    // Sort by hardware byte order to ensure proper ordering for
    // humans.
    return sa1->sa_inet()->address_h() < sa2->sa_inet()->address_h();

  else
    // When we implement INET6 handling, embed the ipv4 address in
    // the ipv6 address.
    throw internal_error("socket_address_key(...) tried to compare an invalid family type.");
}

struct peer_list_equal_port : public std::binary_function<PeerList::reference, uint16_t, bool> {
  bool operator () (PeerList::reference p, uint16_t port) {
    return rak::socket_address::cast_from(p.second->socket_address())->port() == port;
  }
};

PeerList::PeerList() :
  m_availableList(new AvailableList) {
}

PeerList::~PeerList() {
  std::for_each(begin(), end(), rak::on(rak::mem_ref(&value_type::second), rak::call_delete<PeerInfo>()));
  base_type::clear();

  delete m_availableList;
}

PeerInfo*
PeerList::insert_address(const sockaddr* sa, int flags) {
  range_type range = base_type::equal_range(sa);

  // Do some special handling if we got a new port number but the
  // address was present.
  //
  // What we do depends on the flags, but for now just allow one
  // PeerInfo per address key and do nothing.
  if (range.first != range.second)
    return NULL;

  const rak::socket_address* address = rak::socket_address::cast_from(sa);

  PeerInfo* peerInfo = new PeerInfo(sa);
  peerInfo->set_listen_port(address->port());

  base_type::insert(range.second, value_type(socket_address_key(peerInfo->socket_address()), peerInfo));

  if (flags & address_available && peerInfo->listen_port() != 0)
    m_availableList->push_back(address);

  return peerInfo;
}

PeerInfo*
PeerList::connected(const sockaddr* sa, int flags) {
  range_type range = base_type::equal_range(sa);

  PeerInfo* peerInfo;

  if (range.first == range.second) {
    // Create a new entry.
    peerInfo = new PeerInfo(sa);

    base_type::insert(range.second, value_type(socket_address_key(peerInfo->socket_address()), peerInfo));

  } else if (!range.first->second->is_connected()) {
    // Use an old entry.
    peerInfo = range.first->second;
    peerInfo->set_port(rak::socket_address::cast_from(sa)->port());

  } else {
    return NULL;
  }

  if (!(flags & connect_incoming))
    peerInfo->set_listen_port(rak::socket_address::cast_from(sa)->port());

  peerInfo->set_incoming(flags & connect_incoming);

  peerInfo->set_connected(true);
  peerInfo->set_last_connection(cachedTime.seconds());

  return peerInfo;
}

// Make sure we properly clear port when disconnecting.

void
PeerList::disconnected(PeerInfo* p, int flags) {
  range_type range = base_type::equal_range(p->socket_address());
  
  iterator itr = std::find_if(range.first, range.second, rak::equal(p, rak::mem_ref(&value_type::second)));

  if (itr == range.second)
    if (std::find_if(base_type::begin(), base_type::end(), rak::equal(p, rak::mem_ref(&value_type::second))) == base_type::end())
      throw internal_error("PeerList::disconnected(...) itr == range.second, doesn't exist.");
    else
      throw internal_error("PeerList::disconnected(...) itr == range.second, not in the range.");
  
  disconnected(itr, flags);
}

PeerList::iterator
PeerList::disconnected(iterator itr, int flags) {
  if (itr == base_type::end())
    throw internal_error("PeerList::disconnected(...) itr == end().");

  if (!itr->second->is_connected())
    throw internal_error("PeerList::disconnected(...) !itr->is_connected().");

  itr->second->set_connected(false);
  itr->second->set_last_connection(cachedTime.seconds());

  // Replace the socket address port with the listening port so that
  // future outgoing connections will connect to the right port.
  itr->second->set_port(itr->second->listen_port());

  if (flags & disconnect_available && itr->second->listen_port() != 0)
    m_availableList->push_back(rak::socket_address::cast_from(itr->second->socket_address()));

  // Do magic to get rid of unneeded entries.
  return ++itr;
}

}
