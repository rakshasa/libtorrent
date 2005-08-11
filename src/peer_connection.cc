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

#include <cerrno>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "download/download_net.h"

#include "download/download_state.h"
#include "peer_connection.h"
#include "net/manager.h"
#include "settings.h"

namespace torrent {

PeerConnection::PeerConnection() :
  m_shutdown(false),
  m_stallCount(0),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true)
{
  m_taskKeepAlive.set_slot(sigc::mem_fun(*this, &PeerConnection::task_keep_alive));
  m_taskSendChoke.set_slot(sigc::mem_fun(*this, &PeerConnection::task_send_choke));
  m_taskStall.set_slot(sigc::mem_fun(*this, &PeerConnection::task_stall));

  m_taskKeepAlive.set_iterator(taskScheduler.end());
  m_taskSendChoke.set_iterator(taskScheduler.end());
  m_taskStall.set_iterator(taskScheduler.end());
}

PeerConnection::~PeerConnection() {
  if (!get_fd().is_valid())
    return;

  if (m_state == NULL || m_net == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  if (m_read->get_state() != ProtocolRead::BITFIELD)
    m_state->get_bitfield_counter().dec(m_bitfield.get_bitfield());

  taskScheduler.erase(&m_taskKeepAlive);
  taskScheduler.erase(&m_taskSendChoke);
  taskScheduler.erase(&m_taskStall);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);
  
  socketManager.close(get_fd());
  get_fd().clear();
}
  
void
PeerConnection::set_choke(bool v) {
  if (m_write->get_choked() == v)
    return;

  m_sendChoked = true;
  m_write->set_choked(v);
  
  pollCustom->insert_write(this);
}

void
PeerConnection::update_interested() {
  if (m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_read->get_interested();
    m_read->set_interested(true);
  } else {
    m_sendInterested = m_read->get_interested();
    m_read->set_interested(false);
  }
}

void PeerConnection::set(SocketFd fd, const PeerInfo& p, DownloadState* d, DownloadNet* net) {
  if (get_fd().is_valid())
    throw internal_error("Tried to re-set PeerConnection");

  set_fd(fd);
  m_peer = p;
  m_state = d;
  m_net = net;

  get_fd().set_throughput();

  m_requestList.set_delegator(&m_net->get_delegator());
  m_requestList.set_bitfield(&m_bitfield.get_bitfield());

  if (d == NULL || !p.is_valid() || !get_fd().is_valid())
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(d->get_chunk_total());

  if (m_bitfield.begin() == NULL)
    throw internal_error("PeerConnection::set(...) did not properly initialize m_bitfield"); 

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  m_write->get_buffer().reset_position();
  m_read->get_buffer().reset_position();

  if (!d->get_content().get_bitfield().all_zero()) {
    m_write->write_bitfield(m_state->get_content().get_bitfield().size_bytes());

    m_write->get_buffer().prepare_end();
    m_write->set_state(ProtocolWrite::MSG);
  }
    
  taskScheduler.insert(&m_taskKeepAlive, Timer::cache() + 120 * 1000000);
  m_lastMsg = Timer::cache();
}

void PeerConnection::event_read() {
  m_lastMsg = Timer::cache();

  try {
    
  evil_goto_read:
    
  switch (m_read->get_state()) {
  case ProtocolRead::IDLE:
    m_read->get_buffer().reset_position();
    m_read->set_state(ProtocolRead::LENGTH);
    // Reset end when needed

  case ProtocolRead::LENGTH:
    m_read->get_buffer().move_position(read_buf(m_read->get_buffer().position(), 4 - m_read->get_buffer().size_position()));

    if (m_read->get_buffer().size_position() != 4)
      return;

    m_read->get_buffer().reset_position();
    m_read->set_length(m_read->get_buffer().peek_32());

    m_read->get_buffer().set_end(1);

    if (m_read->get_length() == 0) {
      // Received ping.

      m_read->set_state(ProtocolRead::IDLE);
      m_read->set_last_command(ProtocolBase::KEEP_ALIVE);
      return;

    } else if (m_read->get_length() > (1 << 17) + 9) {
      std::stringstream s;
      s << "Recived packet with length 0x" << std::hex << m_read->get_length();

      throw communication_error(s.str());
    }
    
    m_read->set_state(ProtocolRead::TYPE);

    // TMP
    if (m_read->get_buffer().size_position() != 0)
      throw internal_error("Length bork bork bork1");

  // TODO: Read up to 9 bytes or something.
  case ProtocolRead::TYPE:
    if (!read_remaining())
      return;

    m_read->get_buffer().reset_position();

    switch (m_read->get_buffer().peek_8()) {
    case ProtocolBase::REQUEST:
    case ProtocolBase::CANCEL:
      if (m_read->get_length() != 13)
	throw communication_error("Recived request/cancel command with wrong size");

      m_read->get_buffer().set_end(13);
      break;

    case ProtocolBase::HAVE:
      if (m_read->get_length() != 5)
	throw communication_error("Recived have command with wrong size");
      
      m_read->get_buffer().set_end(5);
      break;

    case ProtocolBase::PIECE:
      if (m_read->get_length() < 9 || m_read->get_length() > 9 + (1 << 17))
	throw communication_error("Received piece message with bad length");

      m_read->get_buffer().set_end(9);
      break;

    case ProtocolBase::BITFIELD:
      if (m_read->get_length() != 1 + m_bitfield.size_bytes()) {
	std::stringstream s;

	s << "Recived bitfield message with wrong size " << m_read->get_length()
	  << ' ' << m_bitfield.size_bytes() << ' ';

	throw communication_error(s.str());

      } else if (m_read->get_last_command() != ProtocolBase::NONE) {
	throw communication_error("BitField received after other commands");
      }

      //m_net->signal_network_log().emit("Receiving bitfield");

      //m_read->get_buffer().reset_position();
      m_read->set_position(0);
      m_read->set_state(ProtocolRead::BITFIELD);

      goto evil_goto_read;

    default:
      if (m_read->get_buffer().peek_8() > ProtocolBase::CANCEL)
	throw communication_error("Received unknown protocol command");

      // Handle 1 byte long messages here.
      //m_net->signal_network_log().emit("Receiving some commmand");

      m_read->get_buffer().set_end(1);
      break;
    };

    // Keep the command byte in the buffer.
    m_read->set_state(ProtocolRead::MSG);
    // Read here so the next writes are at the right position.
    m_read->set_last_command((ProtocolBase::Protocol)m_read->get_buffer().read_8());

  case ProtocolRead::MSG:
    if (m_read->get_buffer().remaining() && !read_remaining())
      return;

    m_read->get_buffer().reset_position();

    if (m_read->get_buffer().peek_8() == ProtocolBase::PIECE) {
      m_read->get_buffer().read_8();
      insert_read_throttle();
      receive_piece_header(m_read->read_piece());

    } else {
      parseReadBuf();
      m_read->set_state(ProtocolRead::IDLE);
    }

    goto evil_goto_read;

  case ProtocolRead::BITFIELD:
    m_read->adjust_position(read_buf(m_bitfield.begin() + m_read->get_position(), m_bitfield.size_bytes() - m_read->get_position()));

    if (m_read->get_position() != m_bitfield.size_bytes())
      return;

    m_bitfield.update_count();

    if (!m_bitfield.all_zero() && m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
      m_write->set_interested(m_sendInterested = true);
      
    } else if (m_bitfield.all_set() && m_state->get_content().is_done()) {
      // Both sides are done so we might as well close the connection.
      m_read->set_state(ProtocolRead::INTERNAL_ERROR);
      m_write->set_state(ProtocolWrite::INTERNAL_ERROR);
      throw close_connection();
    }

    m_read->set_state(ProtocolRead::IDLE);
    m_state->get_bitfield_counter().inc(m_bitfield.get_bitfield());

    pollCustom->insert_write(this);
    goto evil_goto_read;

  case ProtocolRead::READ_PIECE:
    if (!m_requestList.is_downloading())
      throw internal_error("ProtocolRead::READ_PIECE state but RequestList is not downloading");

    if (!m_requestList.is_wanted()) {
      m_read->set_state(ProtocolRead::SKIP_PIECE);
      m_read->set_length(m_readChunk.get_bytes_left());
      m_read->set_position(0);

      m_requestList.skip();

      goto evil_goto_read;
    }

    if (!read_chunk())
      return;

    m_read->set_state(ProtocolRead::IDLE);
    m_tryRequest = true;

    m_requestList.finished();
    
    // TODO: Find a way to avoid this remove/insert cycle.
    taskScheduler.erase(&m_taskStall);
    
    if (m_requestList.get_size())
      taskScheduler.insert(&m_taskStall, Timer::cache() + m_state->get_settings().stallTimeout);

    // TODO: clear m_down.data?
    // TODO: remove throttle if choked? Rarely happens though.

    pollCustom->insert_write(this);

    goto evil_goto_read;

  case ProtocolRead::SKIP_PIECE:
    if (m_read->get_position() != 0)
      throw internal_error("ProtocolRead::SKIP_PIECE m_read->get_position() == 0");

    if (m_read->get_length() == 0)
      throw internal_error("ProtocolRead::SKIP_PIECE m_read->get_length() == 0");

    m_read->set_position(read_buf(m_read->get_buffer().begin(),
				 std::min<int>(m_read->get_length(), m_read->get_buffer().reserved())));

    if (m_read->get_position() == 0)
      return;

    m_readRate.insert(m_read->get_position());

    throttleRead.get_rate_slow().insert(m_read->get_position());
    throttleRead.get_rate_quick().insert(m_read->get_position());

    m_read->set_length(m_read->get_length() - m_read->get_position());
    m_read->set_position(0);

    if (m_read->get_length() == 0) {
      // Done with this piece.
      m_read->set_state(ProtocolRead::IDLE);
      m_tryRequest = true;

      taskScheduler.erase(&m_taskStall);

      if (m_requestList.get_size())
	taskScheduler.insert(&m_taskStall, Timer::cache() + m_state->get_settings().stallTimeout);
    }

    goto evil_goto_read;

  default:
    throw internal_error("peer_connectino::read() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->connection_list().erase(this);

  } catch (network_error& e) {
    m_net->signal_network_log().emit(e.what());

    m_net->connection_list().erase(this);

  } catch (storage_error& e) {
    m_state->signal_storage_error().emit(e.what());
    m_net->connection_list().erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ") state(" << m_read->get_state() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::event_write() {
  try {

  evil_goto_write:

  switch (m_write->get_state()) {
  case ProtocolWrite::IDLE:
    m_write->get_buffer().reset_position();

    // Keep alives must set the 5th bit to something IDLE

    if (m_shutdown)
      throw close_connection();

    fillWriteBuf();

    if (m_write->get_buffer().size_position() == 0)
      return pollCustom->remove_write(this);

    m_write->set_state(ProtocolWrite::MSG);
    m_write->get_buffer().prepare_end();

  case ProtocolWrite::MSG:
    if (!write_remaining())
      return;

    switch (m_write->get_last_command()) {
    case ProtocolBase::BITFIELD:
      m_write->set_state(ProtocolWrite::WRITE_BITFIELD);
      m_write->set_position(0);

      goto evil_goto_write;

    case ProtocolBase::PIECE:
      // TODO: Do this somewhere else, and check to see if we are already using the right chunk
      if (m_sendList.empty())
	throw internal_error("Tried writing piece without any requests in list");	  
	
      m_writeChunk.set_chunk(m_state->get_content().get_storage().get_chunk(m_writeChunk.get_piece().get_index(), MemoryChunk::prot_read));
      m_writeChunk.set_position(0);
      m_write->set_state(ProtocolWrite::WRITE_PIECE);

      if (!m_writeChunk.get_chunk().is_valid())
	throw storage_error("Could not create a valid chunk");

      goto evil_goto_write;
      
    default:
      //m_net->signal_network_log().emit("Wrote message to peer");

      m_write->set_state(ProtocolWrite::IDLE);
      return;
    }

  case ProtocolWrite::WRITE_BITFIELD:
    m_write->adjust_position(write_buf(m_state->get_content().get_bitfield().begin() + m_write->get_position(),
				      m_state->get_content().get_bitfield().size_bytes() - m_write->get_position()));

    if (m_write->get_position() == m_state->get_content().get_bitfield().size_bytes())
      m_write->set_state(ProtocolWrite::IDLE);

    return;

  case ProtocolWrite::WRITE_PIECE:
    if (m_sendList.empty())
      throw internal_error("ProtocolWrite::WRITE_PIECE on an empty list");

    if (!write_chunk())
      return;

    if (m_sendList.empty())
      m_writeChunk.get_chunk().clear();

    m_sendList.pop_front();

    m_write->set_state(ProtocolWrite::IDLE);
    return;

  default:
    throw internal_error("PeerConnection::write() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->connection_list().erase(this);

  } catch (network_error& e) {
    m_net->signal_network_log().emit(e.what());
    m_net->connection_list().erase(this);

  } catch (storage_error& e) {
    m_state->signal_storage_error().emit(e.what());
    m_net->connection_list().erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ") state(" << m_write->get_state() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::event_error() {
  m_net->signal_network_log().emit("Connection exception: " + std::string(strerror(errno)));

  m_net->connection_list().erase(this);
}

void PeerConnection::parseReadBuf() {
  ProtocolBase::Protocol curCmd = (ProtocolBase::Protocol)m_read->get_buffer().read_8();

  switch (curCmd) {
  case ProtocolBase::CHOKE:
    if (is_read_choked())
      return;

    m_read->set_choked(true);
    m_requestList.cancel();

    remove_read_throttle();
    taskScheduler.erase(&m_taskStall);

    return;

  case ProtocolBase::UNCHOKE:
    if (!is_read_choked())
      return;

    m_read->set_choked(false);
    m_tryRequest = true;
    
    // We're inserting in read throttle when we receive a piece.
    //insert_read_throttle();

    return pollCustom->insert_write(this);

  case ProtocolBase::INTERESTED:
    m_read->set_interested(true);

    // If we want to send stuff.
    if (m_write->get_choked() &&
	m_net->get_choke_manager().get_max_unchoked() - m_net->get_unchoked() > 0) {
      set_choke(false);
    }
    
    return;

  case ProtocolBase::NOT_INTERESTED:
    m_read->set_interested(false);

    // Choke this uninterested peer and unchoke someone else.
    if (!m_write->get_choked()) {
      set_choke(true);

      m_net->choke_balance();
    }

    return;

  case ProtocolBase::REQUEST:
    if (!m_write->get_choked()) {
      receive_request_piece(m_read->read_request());
      pollCustom->insert_write(this);
    }

    return;

  case ProtocolBase::CANCEL:
    receive_request_piece(m_read->read_request());
    return;

  case ProtocolBase::HAVE:
    receive_have(m_read->get_buffer().read_32());
    return;

  default:
    throw communication_error("Peer sendt unsupported command");
  };
}

void PeerConnection::fillWriteBuf() {
  if (m_sendChoked) {
    m_sendChoked = false;

    if (Timer::cache() - m_lastChoked > 10 * 1000000) {
      // Send choke message immidiately.
      m_write->write_choke(m_write->get_choked());
      m_lastChoked = Timer::cache();

      if (m_write->get_choked()) {
	remove_write_throttle();
	
	m_sendList.clear();
	m_writeChunk.get_chunk().clear();

      } else {
	insert_write_throttle();
      }

    } else if (!taskScheduler.is_scheduled(&m_taskSendChoke)) {
      // Wait with the choke message to avoid oscillation.
      taskScheduler.insert(&m_taskSendChoke, m_lastChoked + 10 * 1000000);
    }
  }

  if (m_sendInterested) {
    m_write->write_interested(m_write->get_interested());
    m_sendInterested = false;
  }

  uint32_t pipeSize;

  if (m_tryRequest && !m_read->get_choked() && m_write->get_interested() &&

      m_read->get_state() != ProtocolRead::SKIP_PIECE &&
      m_net->should_request(m_stallCount) &&
      m_requestList.get_size() < (pipeSize = m_net->pipe_size(m_readRate))) {

    m_tryRequest = false;

    while (m_requestList.get_size() < pipeSize && m_write->can_write_request() && send_request_piece())

      if (m_requestList.get_size() == 1) {
	if (taskScheduler.is_scheduled(&m_taskStall))
	  throw internal_error("Only one request, but we're already in task stall");
	
	taskScheduler.insert(&m_taskStall, Timer::cache() + m_state->get_settings().stallTimeout);
	m_tryRequest = true;
      }	
  }

  // Max buf size 17 * 'req pipe' + 10

  while (!m_haveQueue.empty() && m_write->can_write_have()) {
    m_write->write_have(m_haveQueue.front());
    m_haveQueue.pop_front();
  }

  // Should we send a piece to the peer?
  if (!m_write->get_choked() &&
      !m_sendChoked &&
      !taskScheduler.is_scheduled(&m_taskSendChoke) && // Urgh... find a nicer way to do the delayed choke.
      !m_sendList.empty() &&
      m_write->can_write_piece()) {

    m_writeChunk.set_piece(m_sendList.front());

    // Move these checks somewhere else?
    if (!m_state->get_content().is_valid_piece(m_writeChunk.get_piece()) ||
	!m_state->get_content().has_chunk(m_writeChunk.get_piece().get_index())) {
      std::stringstream s;

      s << "Peer requested a piece with invalid index or length/offset: "
	<< m_writeChunk.get_piece().get_index() << ' '
	<< m_writeChunk.get_piece().get_length() << ' '
	<< m_writeChunk.get_piece().get_offset();

      throw communication_error(s.str());
    }
      
    m_write->write_piece(m_writeChunk.get_piece());
  }
}

void
PeerConnection::receive_have_chunk(int32_t index) {
  m_haveQueue.push_back(index);

  if (m_state->get_content().is_done()) {
    // We're done downloading.

    if (m_bitfield.all_set()) {
      // Peer is done, close connection.
      m_shutdown = true;

    } else {
      m_sendInterested = m_write->get_interested();
      m_write->set_interested(false);
    }

  } else if (m_write->get_interested() &&
	     !m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    // TODO: Optimize?
    m_sendInterested = true;
    m_write->set_interested(false);
  }

  if (m_requestList.has_index(index))
    throw internal_error("PeerConnection::sendHave(...) found a request with the same index");

  // TODO: Also send cancel messages!

  // TODO: Remove this so we group the have messages with other stuff.
  pollCustom->insert_write(this);
}

bool
PeerConnection::send_request_piece() {
  const Piece* p = m_requestList.delegate();

  if (p == NULL)
    return false;

  if (!m_state->get_content().is_valid_piece(*p) || !m_bitfield[p->get_index()])
    throw internal_error("PeerConnection::send_request_piece() tried to use an invalid piece");

  m_write->write_request(*p);

  return true;
}

void
PeerConnection::receive_request_piece(Piece p) {
  PieceList::iterator itr = std::find(m_sendList.begin(), m_sendList.end(), p);
  
  if (itr == m_sendList.end())
    m_sendList.push_back(p);

  pollCustom->insert_write(this);
}

void
PeerConnection::receive_cancel_piece(Piece p) {
  PieceList::iterator itr = std::find(m_sendList.begin(), m_sendList.end(), p);
  
  if (itr != m_sendList.begin() && m_write->get_state() == ProtocolWrite::IDLE)
    m_sendList.erase(itr);
}  

void
PeerConnection::receive_have(uint32_t index) {
  if (index >= m_bitfield.size_bits())
    throw communication_error("Recieved HAVE command with invalid value");

  if (m_bitfield[index])
    return;

  m_bitfield.set(index, true);
  m_state->get_bitfield_counter().inc(index);

  m_peerRate.insert(m_state->get_content().get_storage().get_chunk_size());
    
  if (m_state->get_content().is_done())
    return;

  if (!m_write->get_interested() && m_net->get_delegator().get_select().interested(index)) {
    m_sendInterested = true;
    m_write->set_interested(true);

    pollCustom->insert_write(this);
  }

  // Make sure m_tryRequest is set even if we were previously
  // interested. Super-Seeders seem to cause it to stall while we
  // are interested, but m_tryRequest is cleared.
  m_tryRequest = true;
}

void
PeerConnection::receive_piece_header(Piece p) {
  if (p.get_length() == 0) {
    // Some clients send zero length messages when we request pieces
    // they don't have.
    m_net->signal_network_log().emit("Received piece with length zero");
    m_read->set_state(ProtocolRead::IDLE);

    return;
  }

  if (m_requestList.downloading(p)) {
    m_read->set_state(ProtocolRead::READ_PIECE);
    m_readChunk.set_position(0);
    load_read_chunk(p);
  } else {
    // We don't want this piece,
    m_read->set_state(ProtocolRead::SKIP_PIECE);
    m_read->set_length(p.get_length());
    m_read->set_position(0);
  }
}  

void
PeerConnection::task_keep_alive() {
  // Check if remote peer is dead.
  if (Timer::cache() - m_lastMsg > 240 * 1000000) {
    m_net->connection_list().erase(this);
    return;
  }

  // Send keep-alive only if we arn't already transmitting. There's no
  // point to send it if there is data being sendt, and we don't
  // bother with a timer for the last sendt message since it is
  // cleaner just to send this message.

  if (m_write->get_state() == ProtocolWrite::IDLE) {
    m_write->get_buffer().reset_position();
    m_write->write_keepalive();

    m_write->get_buffer().prepare_end();
    m_write->set_state(ProtocolWrite::MSG);

    pollCustom->insert_write(this);
  }

  m_tryRequest = true;
  taskScheduler.insert(&m_taskKeepAlive, Timer::cache() + 120 * 1000000);
}

void
PeerConnection::task_send_choke() {
  m_sendChoked = true;

  pollCustom->insert_write(this);
}

void
PeerConnection::task_stall() {
  m_stallCount++;
  m_requestList.stall();

  // Make sure we regulary call task_stall() so stalled queues with new
  // entries get those new ones stalled if needed.
  taskScheduler.insert(&m_taskStall, Timer::cache() + m_state->get_settings().stallTimeout);
}

}
