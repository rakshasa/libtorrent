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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "exceptions.h"
#include "peer.h"
#include "peer_connection.h"

namespace torrent {

std::string
Peer::get_id() {
  return m_ptr->peer().get_id();
}

std::string
Peer::get_dns() {
  return m_ptr->peer().get_dns();
}

uint16_t
Peer::get_port() {
  return m_ptr->peer().get_port();
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
//   return m_ptr->throttle().get_snub();
  return false;
}

// Bytes per second.
uint32_t
Peer::get_rate_down() {
  return m_ptr->get_rate_down().rate();
}

uint32_t
Peer::get_rate_up() {
  return m_ptr->get_rate_up().rate();
}

uint32_t
Peer::get_rate_peer() {
  return m_ptr->get_rate_peer().rate();
}

uint64_t
Peer::get_transfered_down() {
  return m_ptr->get_rate_down().total();
}  

uint64_t
Peer::get_transfered_up() {
  return m_ptr->get_rate_up().total();
}  

uint32_t
Peer::get_incoming_queue_size() {
  return m_ptr->get_requests().get_size();
}

uint32_t
Peer::get_outgoing_queue_size() {
  return m_ptr->get_sends().size();
}  

uint32_t
Peer::get_incoming_index(uint32_t pos) {
  if (pos >= m_ptr->get_requests().get_size())
    throw client_error("get_incoming_index(pos) out of range");

  return m_ptr->get_requests().get_queued_piece(pos).get_index();
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
//   m_ptr->throttle().set_snub(v);

//   if (v)
//     m_ptr->choke(true);
}

}
