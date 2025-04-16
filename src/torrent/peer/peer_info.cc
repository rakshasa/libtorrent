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

#include <cstring>
#include <rak/socket_address.h>

#include "protocol/extensions.h"
#include "protocol/peer_connection_base.h"
#include "utils/instrumentation.h"

#include "exceptions.h"
#include "peer_info.h"

namespace torrent {

// Move this to peer_info.cc when these are made into the public API.
//
// TODO: Use a safer socket address parameter.
PeerInfo::PeerInfo(const sockaddr* address) {
  auto sa = new rak::socket_address();
  *sa = *rak::socket_address::cast_from(address);

  m_address = sa->c_sockaddr();
}

PeerInfo::~PeerInfo() {
  // if (m_transferCounter != 0)
  //   throw internal_error("PeerInfo::~PeerInfo() m_transferCounter != 0.");

  instrumentation_update(INSTRUMENTATION_TRANSFER_PEER_INFO_UNACCOUNTED, m_transferCounter);

  if (is_blocked())
    throw internal_error("PeerInfo::~PeerInfo() peer is blocked.");

  delete rak::socket_address::cast_from(m_address);
}

void
PeerInfo::set_port(uint16_t port) {
  rak::socket_address::cast_from(m_address)->set_port(port);
}

}
