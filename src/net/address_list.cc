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

#include "config.h"

#include <algorithm>
#include <rak/functional.h>

#include "address_list.h"

namespace torrent {

inline rak::socket_address
AddressList::parse_address(const Object& b) {
  rak::socket_address sa;
  sa.clear();

  if (!b.is_map())
    return sa;

  if (!b.has_key_string("ip") || !sa.set_address_str(b.get_key_string("ip")))
    return sa;

  if (!b.has_key_value("port") || b.get_key_value("port") <= 0 || b.get_key_value("port") >= (1 << 16))
    return sa;

  sa.set_port(b.get_key_value("port"));

  return sa;
}

void
AddressList::parse_address_normal(const Object::list_type& b) {
  std::for_each(b.begin(), b.end(), rak::on(std::ptr_fun(&AddressList::parse_address), AddressList::add_address(this)));
}

void
AddressList::parse_address_compact(raw_string s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("ConnectionList::AddressList::parse_address_compact(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact*>(s.data()),
	    reinterpret_cast<const SocketAddressCompact*>(s.data() + s.size() - s.size() % sizeof(SocketAddressCompact)),
	    std::back_inserter(*this));
}

#ifdef RAK_USE_INET6
void
AddressList::parse_address_compact_ipv6(const std::string& s) {
  if (sizeof(const SocketAddressCompact6) != 18)
    throw internal_error("ConnectionList::AddressList::parse_address_compact_ipv6(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact6*>(s.c_str()),
            reinterpret_cast<const SocketAddressCompact6*>(s.c_str() + s.size() - s.size() % sizeof(SocketAddressCompact6)),
            std::back_inserter(*this));
}
#endif

void
AddressList::parse_address_bencode(raw_list s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("AddressList::parse_address_bencode(...) bad struct size.");

  for (raw_list::const_iterator itr = s.begin();
       itr + 2 + sizeof(SocketAddressCompact) <= s.end();
       itr += sizeof(SocketAddressCompact)) {
    if (*itr++ != '6' || *itr++ != ':')
      break;

    insert(end(), *reinterpret_cast<const SocketAddressCompact*>(s.data()));
  }
}

}
