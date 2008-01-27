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

bool Peer::is_encrypted() const            { return c_ptr()->is_encrypted(); }
bool Peer::is_obfuscated() const           { return c_ptr()->is_obfuscated(); }

bool Peer::is_up_choked() const            { return c_ptr()->is_up_choked(); }
bool Peer::is_up_interested() const        { return c_ptr()->is_down_interested(); }

bool Peer::is_down_choked() const          { return !c_ptr()->is_down_remote_unchoked(); }
bool Peer::is_down_choked_limited() const  { return !c_ptr()->is_down_local_unchoked(); }
bool Peer::is_down_queued() const          { return c_ptr()->is_down_queued(); }
bool Peer::is_down_interested() const      { return c_ptr()->is_up_interested(); }

bool Peer::is_snubbed() const              { return c_ptr()->is_up_snubbed(); }
void Peer::set_snubbed(bool v)             { m_ptr()->set_upload_snubbed(v); }

const Rate*       Peer::down_rate() const  { return c_ptr()->c_peer_chunks()->download_throttle()->rate(); } 
const Rate*       Peer::up_rate() const    { return c_ptr()->c_peer_chunks()->upload_throttle()->rate(); } 
const Rate*       Peer::peer_rate() const  { return c_ptr()->c_peer_chunks()->peer_rate(); } 

const Bitfield*   Peer::bitfield() const   { return c_ptr()->c_peer_chunks()->bitfield(); }

uint32_t Peer::incoming_queue_size() const { return c_ptr()->download_queue()->queued_size(); }
uint32_t Peer::outgoing_queue_size() const { return c_ptr()->c_peer_chunks()->upload_queue()->size(); }  
uint32_t Peer::chunks_done() const         { return c_ptr()->c_peer_chunks()->bitfield()->size_set(); }  

const BlockTransfer*
Peer::transfer() const {
  if (c_ptr()->download_queue()->transfer() != NULL)
    return c_ptr()->download_queue()->transfer();

  else if (!c_ptr()->download_queue()->queued_empty())
    return c_ptr()->download_queue()->queued_transfer(0);

  else
    return NULL;
}

}
