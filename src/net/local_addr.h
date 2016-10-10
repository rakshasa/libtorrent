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

// A routine to get a local IP address that can be presented to a tracker.
// (Does not use UPnP etc., so will not understand NAT.)
// On a machine with multiple network cards, address selection can be a
// complex process, and in general what's selected is a source/destination
// address pair. However, this routine will give an approximation that will
// be good enough for most purposes and users.

#ifndef LIBTORRENT_NET_LOCAL_ADDR_H
#define LIBTORRENT_NET_LOCAL_ADDR_H

#include <unistd.h>

namespace rak {
  class socket_address;
}

namespace torrent {

// Note: family must currently be rak::af_inet or rak::af_inet6
// (rak::af_unspec won't do); anything else will throw an exception.
// Returns false if no address of the given family could be found,
// either because there are none, or because something went wrong in
// the process (e.g., no free file descriptors).
bool get_local_address(sa_family_t family, rak::socket_address *address);

}

#endif /* LIBTORRENT_NET_LOCAL_ADDR_H */
