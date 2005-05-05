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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <netinet/in.h>
#include <sstream>

#include "torrent/exceptions.h"
#include "download/download_state.h"
#include "download/download_net.h"
#include "net/poll.h"

#include "peer_connection.h"

// This file is for functions that does not directly relate to the
// protocol logic. Or the split might be completely arbitary...

namespace torrent {

PeerConnection::PeerConnection() :
  m_shutdown(false),
  m_stallCount(0),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true),
  
  m_taskKeepAlive(sigc::mem_fun(*this, &PeerConnection::task_keep_alive)),
  m_taskSendChoke(sigc::mem_fun(*this, &PeerConnection::task_send_choke)),
  m_taskStall(sigc::mem_fun(*this, &PeerConnection::task_stall))
{
}

PeerConnection::~PeerConnection() {
  if (m_state) {
    if (m_requests.is_downloading())
      m_requests.skip();

    m_requests.cancel();

    if (m_read.get_state() != ProtocolRead::BITFIELD)
      m_state->get_bitfield_counter().dec(m_bitfield.get_bitfield());
  }

  if (!m_fd.is_valid())
    return;

  Poll::read_set().erase(this);
  Poll::write_set().erase(this);
  Poll::except_set().erase(this);
  
  m_fd.close();
  m_fd.clear();
}
  
bool
PeerConnection::send_request_piece() {
  const Piece* p;

  if ((p = m_requests.delegate()) == NULL)
    return false;

  if (!m_state->get_content().is_valid_piece(*p) ||
      !m_bitfield[p->get_index()]) {
    std::stringstream s;
    
    s << "Tried to request an invalid piece: "
      << p->get_index() << ' '
      << p->get_length() << ' '
      << p->get_offset();
    
    throw internal_error(s.str());
  }

  m_write.write_request(*p);

  return true;
}

void
PeerConnection::receive_request_piece(Piece p) {
  SendList::iterator itr = std::find(m_sends.begin(), m_sends.end(), p);
  
  if (itr == m_sends.end())
    m_sends.push_back(p);

  Poll::write_set().insert(this);
}

void
PeerConnection::receive_cancel_piece(Piece p) {
  SendList::iterator itr = std::find(m_sends.begin(), m_sends.end(), p);
  
  if (itr != m_sends.begin() && m_write.get_state() == ProtocolWrite::IDLE)
    m_sends.erase(itr);
}  

void
PeerConnection::receive_have(uint32_t index) {
  if (index >= m_bitfield.size_bits())
    throw communication_error("Recived HAVE command with invalid value");

  if (!m_bitfield[index]) {
    m_bitfield.set(index, true);
    m_state->get_bitfield_counter().inc(index);
  }
    
  if (!m_write.get_interested() && m_net->get_delegator().get_select().interested(index)) {
    // We are interested, send flag if not already set.
    m_sendInterested = true;
    m_write.set_interested(true);

    Poll::write_set().insert(this);
  }

  // Make sure m_tryRequest is set even if we were previously
  // interested. Super-Seeders seem to cause it to stall while we
  // are interested, but m_tryRequest is cleared.
  m_tryRequest = true;
  m_peerRate.insert(m_state->get_content().get_storage().get_chunk_size());
}

void PeerConnection::choke(bool v) {
  if (m_write.get_choked() == v)
    return;

  m_sendChoked = true;
  m_write.set_choked(v);
  
  Poll::write_set().insert(this);
}

void
PeerConnection::update_interested() {
  if (m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_read.get_interested();
    m_read.set_interested(true);
  } else {
    m_sendInterested = m_read.get_interested();
    m_read.set_interested(false);
  }
}

} // namespace torrent
