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

#include "config.h"

#include "exceptions.h"
#include "peer.h"
#include "rate.h"
#include "peer_connection.h"

namespace torrent {

std::string
Peer::get_id() {
  return m_ptr->get_peer().get_id();
}

std::string
Peer::get_dns() {
  return m_ptr->get_peer().get_address();
}

uint16_t
Peer::get_port() {
  return m_ptr->get_peer().get_port();
}

const char*
Peer::get_options() {
  return m_ptr->get_peer().get_options();
}

bool
Peer::get_local_choked() {
  return m_ptr->is_write_choked();
}

bool
Peer::get_local_interested() {
  return m_ptr->is_write_interested();
}

bool
Peer::get_remote_choked() {
  return m_ptr->is_read_choked();
}

bool
Peer::get_remote_interested() {
  return m_ptr->is_read_interested();
}

bool
Peer::get_snubbed() {
  return m_ptr->is_snubbed();
}

const Rate&
Peer::get_read_rate() {
  return m_ptr->get_read_rate();
} 

const Rate&
Peer::get_write_rate() {
  return m_ptr->get_write_rate();
} 

const Rate&
Peer::get_peer_rate() {
  return m_ptr->get_peer_rate();
} 

uint32_t
Peer::get_incoming_queue_size() {
  return m_ptr->get_request_list().get_size();
}

uint32_t
Peer::get_outgoing_queue_size() {
  return m_ptr->get_send_list().size();
}  

uint32_t
Peer::get_incoming_index(uint32_t pos) {
  if (pos >= m_ptr->get_request_list().get_size())
    throw client_error("get_incoming_index(pos) out of range");

  return m_ptr->get_request_list().get_queued_piece(pos).get_index();
}

  uint32_t             get_incoming_offset(uint32_t pos);
  uint32_t             get_incoming_length(uint32_t pos);

// Currently needs to copy the data once to a std::string. But 
// since gcc does ref counted std::string, you can inexpensively
// copy the resulting string. Will consider making BitField use a
// std::string.
const unsigned char*
Peer::get_bitfield_data() {
  return (const unsigned char*)m_ptr->get_bitfield().begin();
}

uint32_t
Peer::get_bitfield_size() {
  return m_ptr->get_bitfield().size_bits();
}

uint32_t
Peer::get_chunks_done() {
  return m_ptr->get_bitfield().count();
}  

void
Peer::set_snubbed(bool v) {
  m_ptr->set_snubbed(v);

  if (v)
    m_ptr->set_choke(true);
}

}
