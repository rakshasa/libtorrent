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

#include "data/block.h"
#include "data/block_transfer.h"
#include "download/choke_manager.h"
#include "download/download_main.h"
#include "protocol/peer_chunks.h"
#include "protocol/peer_connection_base.h"

#include "exceptions.h"
#include "peer.h"
#include "peer_info.h"
#include "rate.h"

namespace torrent {

bool
Peer::is_incoming() const {
  return m_ptr->peer_info()->is_incoming();
}

bool
Peer::is_encrypted() const {
  return m_ptr->is_encrypted();
}

bool
Peer::is_obfuscated() const {
  return m_ptr->is_obfuscated();
}

bool
Peer::is_local_choked() const {
  return m_ptr->is_up_choked();
}

bool
Peer::is_local_interested() const {
  return m_ptr->is_up_interested();
}

bool
Peer::is_remote_choked() const {
  return m_ptr->is_down_choked();
}

bool
Peer::is_remote_interested() const {
  return m_ptr->is_down_interested();
}

bool
Peer::is_snubbed() const {
  return m_ptr->peer_chunks()->is_snubbed();
}

void
Peer::set_snubbed(bool v) {
  if (v)
    m_ptr->download()->upload_choke_manager()->set_snubbed(m_ptr);
  else
    m_ptr->download()->upload_choke_manager()->set_not_snubbed(m_ptr);
}

const HashString&
Peer::id() const {
  return m_ptr->peer_info()->id();
}

const char*
Peer::options() const {
  return m_ptr->peer_info()->options();
}

const sockaddr*
Peer::address() const {
  return m_ptr->peer_info()->socket_address();
}

const Rate*
Peer::down_rate() const {
  return m_ptr->peer_chunks()->download_throttle()->rate();
} 

const Rate*
Peer::up_rate() const {
  return m_ptr->peer_chunks()->upload_throttle()->rate();
} 

const Rate*
Peer::peer_rate() const {
  return m_ptr->peer_chunks()->peer_rate();
} 

const PeerInfo*
Peer::info() const {
  return m_ptr->peer_info();
}

const Bitfield*
Peer::bitfield() const {
  return m_ptr->peer_chunks()->bitfield();
}

const BlockTransfer*
Peer::transfer() const {
  if (m_ptr->download_queue()->transfer() != NULL)
    return m_ptr->download_queue()->transfer();

  else if (!m_ptr->download_queue()->empty())
    return m_ptr->download_queue()->queued_transfer(0);

  else
    return NULL;
}

uint32_t
Peer::incoming_queue_size() const {
  return m_ptr->download_queue()->size();
}

uint32_t
Peer::outgoing_queue_size() const {
  return m_ptr->peer_chunks()->upload_queue()->size();
}  

uint32_t
Peer::chunks_done() const {
  return m_ptr->peer_chunks()->bitfield()->size_set();
}  

uint32_t
Peer::failed_counter() const {
  return m_ptr->peer_info()->failed_counter();
}  

}
