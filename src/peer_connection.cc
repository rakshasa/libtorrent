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
#include "download/download_main.h"
#include "peer_connection.h"
#include "net/manager.h"

namespace torrent {

// Temporary optimization test functions.
uint16_t test_buffer_read16(ProtocolBuffer<512>* pb) {
  return pb->read_16();
}

uint32_t test_buffer_read32(ProtocolBuffer<512>* pb) {
  return pb->read_32();
}

uint64_t test_buffer_read64(ProtocolBuffer<512>* pb) {
  return pb->read_64();
}

void test_buffer_write16(ProtocolBuffer<512>* pb, uint16_t v) {
  return pb->write_16(v);
}

void test_buffer_write32(ProtocolBuffer<512>* pb, uint32_t v) {
  return pb->write_32(v);
}

void test_buffer_write64(ProtocolBuffer<512>* pb, uint64_t v) {
  return pb->write_64(v);
}

PeerConnection::PeerConnection() :
  m_shutdown(false),

  m_sendInterested(false),
  m_tryRequest(true)
{
  m_taskSendChoke.set_iterator(taskScheduler.end());
  m_taskSendChoke.set_slot(sigc::mem_fun(*this, &PeerConnection::task_send_choke));
}

PeerConnection::~PeerConnection() {
  if (!get_fd().is_valid())
    return;

  if (m_download == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  if (m_down->get_state() != ProtocolRead::READ_BITFIELD)
    m_download->get_bitfield_counter().dec(m_bitfield.get_bitfield());

  taskScheduler.erase(&m_taskSendChoke);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);
  
  socketManager.close(get_fd());
  get_fd().clear();
}
  
void
PeerConnection::set_choke(bool v) {
  if (m_up->get_choked() == v)
    return;

  m_sendChoked = true;
  m_up->set_choked(v);
  
  pollCustom->insert_write(this);
}

void
PeerConnection::update_interested() {
  if (m_download->delegator()->get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_down->get_interested();
    m_down->set_interested(true);
  } else {
    m_sendInterested = m_down->get_interested();
    m_down->set_interested(false);
  }
}

void PeerConnection::set(SocketFd fd, const PeerInfo& p, DownloadMain* download) {
  if (get_fd().is_valid())
    throw internal_error("Tried to re-set PeerConnection");

  set_fd(fd);
  m_peer = p;
  m_download = download;

  get_fd().set_throughput();

  m_requestList.set_delegator(m_download->delegator());
  m_requestList.set_bitfield(&m_bitfield.get_bitfield());

  if (m_download == NULL || !p.is_valid() || !get_fd().is_valid())
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(m_download->content()->get_chunk_total());

  if (m_bitfield.begin() == NULL)
    throw internal_error("PeerConnection::set(...) did not properly initialize m_bitfield"); 

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  m_up->get_buffer().reset_position();
  m_down->get_buffer().reset_position();

  if (m_download->content()->get_chunks_completed() != 0) {
    m_up->write_bitfield(m_download->content()->get_bitfield().size_bytes());

    m_up->get_buffer().prepare_end();
    m_up->set_state(ProtocolWrite::MSG);
  }
    
  m_timeLastRead = Timer::cache();
}

void PeerConnection::event_read() {
  m_timeLastRead = Timer::cache();

  try {
    
  evil_goto_read:
    
  switch (m_down->get_state()) {
  case ProtocolRead::IDLE:
    m_down->get_buffer().reset_position();
    m_down->set_state(ProtocolRead::LENGTH);
    // Reset end when needed

  case ProtocolRead::LENGTH:
    m_down->get_buffer().move_position(read_buf(m_down->get_buffer().position(), 4 - m_down->get_buffer().size_position()));

    if (m_down->get_buffer().size_position() != 4)
      return;

    m_down->get_buffer().reset_position();
    m_down->set_length(m_down->get_buffer().peek_32());

    m_down->get_buffer().set_end(1);

    if (m_down->get_length() == 0) {
      // Received ping.

      m_down->set_state(ProtocolRead::IDLE);
      m_down->set_last_command(ProtocolBase::KEEP_ALIVE);
      return;

    } else if (m_down->get_length() > (1 << 17) + 9) {
      std::stringstream s;
      s << "Recived packet with length 0x" << std::hex << m_down->get_length();

      throw communication_error(s.str());
    }
    
    m_down->set_state(ProtocolRead::TYPE);

    // TMP
    if (m_down->get_buffer().size_position() != 0)
      throw internal_error("Length bork bork bork1");

  // TODO: Read up to 9 bytes or something.
  case ProtocolRead::TYPE:
    if (!read_remaining())
      return;

    m_down->get_buffer().reset_position();

    switch (m_down->get_buffer().peek_8()) {
    case ProtocolBase::REQUEST:
    case ProtocolBase::CANCEL:
      if (m_down->get_length() != 13)
	throw communication_error("Recived request/cancel command with wrong size");

      m_down->get_buffer().set_end(13);
      break;

    case ProtocolBase::HAVE:
      if (m_down->get_length() != 5)
	throw communication_error("Recived have command with wrong size");
      
      m_down->get_buffer().set_end(5);
      break;

    case ProtocolBase::PIECE:
      if (m_down->get_length() < 9 || m_down->get_length() > 9 + (1 << 17))
	throw communication_error("Received piece message with bad length");

      m_down->get_buffer().set_end(9);
      break;

    case ProtocolBase::BITFIELD:
      if (m_down->get_length() != 1 + m_bitfield.size_bytes()) {
	std::stringstream s;

	s << "Recived bitfield message with wrong size " << m_down->get_length()
	  << ' ' << m_bitfield.size_bytes() << ' ';

	throw communication_error(s.str());

      } else if (m_down->get_last_command() != ProtocolBase::NONE) {
	throw communication_error("BitField received after other commands");
      }

      //m_download->net()->signal_network_log().emit("Receiving bitfield");

      //m_down->get_buffer().reset_position();
      m_down->set_position(0);
      m_down->set_state(ProtocolRead::READ_BITFIELD);

      goto evil_goto_read;

    default:
      if (m_down->get_buffer().peek_8() > ProtocolBase::CANCEL)
	throw communication_error("Received unknown protocol command");

      // Handle 1 byte long messages here.
      //m_download->net()->signal_network_log().emit("Receiving some commmand");

      m_down->get_buffer().set_end(1);
      break;
    };

    // Keep the command byte in the buffer.
    m_down->set_state(ProtocolRead::MSG);
    // Read here so the next writes are at the right position.
    m_down->set_last_command((ProtocolBase::Protocol)m_down->get_buffer().read_8());

  case ProtocolRead::MSG:
    if (m_down->get_buffer().remaining() && !read_remaining())
      return;

    m_down->get_buffer().reset_position();

    if (m_down->get_buffer().peek_8() == ProtocolBase::PIECE) {
      m_down->get_buffer().read_8();
      insert_down_throttle();
      receive_piece_header(m_down->read_piece());

    } else {
      parseReadBuf();
      m_down->set_state(ProtocolRead::IDLE);
    }

    goto evil_goto_read;

  case ProtocolRead::READ_BITFIELD:
    m_down->adjust_position(read_buf(m_bitfield.begin() + m_down->get_position(), m_bitfield.size_bytes() - m_down->get_position()));

    if (m_down->get_position() != m_bitfield.size_bytes())
      return;

    m_bitfield.update_count();

    if (!m_bitfield.all_zero() && m_download->delegator()->get_select().interested(m_bitfield.get_bitfield())) {
      m_up->set_interested(m_sendInterested = true);
      
    } else if (m_bitfield.all_set() && m_download->content()->is_done()) {
      // Both sides are done so we might as well close the connection.
      m_down->set_state(ProtocolRead::INTERNAL_ERROR);
      m_up->set_state(ProtocolWrite::INTERNAL_ERROR);
      throw close_connection();
    }

    m_down->set_state(ProtocolRead::IDLE);
    m_download->get_bitfield_counter().inc(m_bitfield.get_bitfield());

    pollCustom->insert_write(this);
    goto evil_goto_read;

  case ProtocolRead::READ_PIECE:
    if (!m_requestList.is_downloading())
      throw internal_error("ProtocolRead::READ_PIECE state but RequestList is not downloading");

    if (!m_requestList.is_wanted()) {
      m_down->set_state(ProtocolRead::SKIP_PIECE);
      m_down->set_length(m_downPiece.get_length() - m_down->get_position());
      m_down->set_position(0);

      m_requestList.skip();

      goto evil_goto_read;
    }

    if (!down_chunk())
      return;

    m_down->set_state(ProtocolRead::IDLE);
    m_tryRequest = true;

    m_requestList.finished();
    
    if (m_downStall > 0)
      m_downStall--;
    
    // TODO: clear m_down.data?
    // TODO: remove throttle if choked? Rarely happens though.

    pollCustom->insert_write(this);

    goto evil_goto_read;

  case ProtocolRead::SKIP_PIECE:
    if (m_down->get_position() != 0)
      throw internal_error("ProtocolRead::SKIP_PIECE m_down->get_position() == 0");

    if (m_down->get_length() == 0)
      throw internal_error("ProtocolRead::SKIP_PIECE m_down->get_length() == 0");

    m_down->set_position(read_buf(m_down->get_buffer().begin(),
				 std::min<int>(m_down->get_length(), m_down->get_buffer().reserved())));

    if (m_down->get_position() == 0)
      return;

    m_downRate.insert(m_down->get_position());

    throttleRead.get_rate_slow().insert(m_down->get_position());
    throttleRead.get_rate_quick().insert(m_down->get_position());

    m_down->set_length(m_down->get_length() - m_down->get_position());
    m_down->set_position(0);

    if (m_down->get_length() == 0) {
      // Done with this piece.
      m_down->set_state(ProtocolRead::IDLE);
      m_tryRequest = true;

      if (m_downStall > 0)
	m_downStall--;
    }

    goto evil_goto_read;

  default:
    throw internal_error("peer_connectino::read() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->signal_network_log().emit(e.what());

    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ") state(" << m_down->get_state() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::event_write() {
  try {

  evil_goto_write:

  switch (m_up->get_state()) {
  case ProtocolWrite::IDLE:
    m_up->get_buffer().reset_position();

    // Keep alives must set the 5th bit to something IDLE

    if (m_shutdown)
      throw close_connection();

    fillWriteBuf();

    if (m_up->get_buffer().size_position() == 0)
      return pollCustom->remove_write(this);

    m_up->set_state(ProtocolWrite::MSG);
    m_up->get_buffer().prepare_end();

  case ProtocolWrite::MSG:
    if (!write_remaining())
      return;

    switch (m_up->get_last_command()) {
    case ProtocolBase::BITFIELD:
      m_up->set_state(ProtocolWrite::WRITE_BITFIELD);
      m_up->set_position(0);

      goto evil_goto_write;

    case ProtocolBase::PIECE:
      // TODO: Do this somewhere else, and check to see if we are already using the right chunk
      if (m_sendList.empty())
	throw internal_error("Tried writing piece without any requests in list");	  
	
      m_up->set_state(ProtocolWrite::WRITE_PIECE);
      m_up->set_position(0);

      load_up_chunk();
      goto evil_goto_write;
      
    default:
      //m_download->net()->signal_network_log().emit("Wrote message to peer");

      m_up->set_state(ProtocolWrite::IDLE);
      return;
    }

  case ProtocolWrite::WRITE_BITFIELD:
    m_up->adjust_position(write_buf(m_download->content()->get_bitfield().begin() + m_up->get_position(),
				      m_download->content()->get_bitfield().size_bytes() - m_up->get_position()));

    if (m_up->get_position() == m_download->content()->get_bitfield().size_bytes())
      m_up->set_state(ProtocolWrite::IDLE);

    return;

  case ProtocolWrite::WRITE_PIECE:
    if (m_sendList.empty())
      throw internal_error("ProtocolWrite::WRITE_PIECE on an empty list");

    if (!up_chunk())
      return;

    if (m_sendList.empty()) {
      m_download->content()->release_chunk(m_upChunk);
      m_upChunk = NULL;
    }

    m_sendList.pop_front();

    m_up->set_state(ProtocolWrite::IDLE);
    return;

  default:
    throw internal_error("PeerConnection::write() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->signal_network_log().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ") state(" << m_up->get_state() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::event_error() {
  m_download->signal_network_log().emit("Connection exception: " + std::string(strerror(errno)));

  m_download->connection_list()->erase(this);
}

void PeerConnection::parseReadBuf() {
  ProtocolBase::Protocol curCmd = (ProtocolBase::Protocol)m_down->get_buffer().read_8();

  switch (curCmd) {
  case ProtocolBase::CHOKE:
    if (is_down_choked())
      return;

    m_down->set_choked(true);
    m_requestList.cancel();

    remove_down_throttle();
    return;

  case ProtocolBase::UNCHOKE:
    if (!is_down_choked())
      return;

    m_down->set_choked(false);
    m_tryRequest = true;

    pollCustom->insert_write(this);
    return;

  case ProtocolBase::INTERESTED:
    m_down->set_interested(true);

    // If we want to send stuff.
    if (m_up->get_choked() &&
	(int)m_download->choke_manager()->get_max_unchoked() - m_download->size_unchoked() > 0) {
      set_choke(false);
    }
    
    return;

  case ProtocolBase::NOT_INTERESTED:
    m_down->set_interested(false);

    // Choke this uninterested peer and unchoke someone else.
    if (!m_up->get_choked()) {
      set_choke(true);

      m_download->choke_balance();
    }

    return;

  case ProtocolBase::REQUEST:
    if (!m_up->get_choked()) {
      read_request_piece(m_down->read_request());
      pollCustom->insert_write(this);
    }

    return;

  case ProtocolBase::CANCEL:
    read_cancel_piece(m_down->read_request());
    return;

  case ProtocolBase::HAVE:
    receive_have(m_down->get_buffer().read_32());
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
      m_up->write_choke(m_up->get_choked());
      m_lastChoked = Timer::cache();

      if (m_up->get_choked()) {
	remove_up_throttle();
	
	m_sendList.clear();

	if (m_upChunk != NULL) {
	  m_download->content()->release_chunk(m_upChunk);
	  m_upChunk = NULL;
	}

      } else {
	insert_up_throttle();
      }

    } else if (!taskScheduler.is_scheduled(&m_taskSendChoke)) {
      // Wait with the choke message to avoid oscillation.
      taskScheduler.insert(&m_taskSendChoke, (m_lastChoked + 10 * 1000000).round_seconds());
    }
  }

  if (m_sendInterested) {
    m_up->write_interested(m_up->get_interested());
    m_sendInterested = false;
  }

  uint32_t pipeSize;

  if (m_tryRequest && !m_down->get_choked() && m_up->get_interested() &&

      m_down->get_state() != ProtocolRead::SKIP_PIECE &&
      should_request() &&
      m_requestList.get_size() < (pipeSize = pipe_size())) {

    m_tryRequest = false;

    while (m_requestList.get_size() < pipeSize && m_up->can_write_request() && send_request_piece()) {

      if (m_requestList.get_size() == 1) {
	m_downStall = 0;
	m_tryRequest = true;
      }	
    }
  }

  // Max buf size 17 * 'req pipe' + 10

  while (!m_haveQueue.empty() && m_up->can_write_have()) {
    m_up->write_have(m_haveQueue.front());
    m_haveQueue.pop_front();
  }

  // Should we send a piece to the peer?
  if (!m_up->get_choked() &&
      !m_sendChoked && // Why sendchoked here?
      !taskScheduler.is_scheduled(&m_taskSendChoke) && // Urgh... find a nicer way to do the delayed choke.
      !m_sendList.empty() &&
      m_up->can_write_piece()) {

    m_upPiece = m_sendList.front();

    // Move these checks somewhere else?
    if (!m_download->content()->is_valid_piece(m_upPiece) ||
	!m_download->content()->has_chunk(m_upPiece.get_index())) {
      std::stringstream s;

      s << "Peer requested a piece with invalid index or length/offset: "
	<< m_upPiece.get_index() << ' '
	<< m_upPiece.get_length() << ' '
	<< m_upPiece.get_offset();

      throw communication_error(s.str());
    }
      
    m_up->write_piece(m_upPiece);
  }
}

void
PeerConnection::receive_have_chunk(int32_t index) {
  m_haveQueue.push_back(index);

  if (m_download->content()->is_done()) {
    // We're done downloading.

    if (m_bitfield.all_set()) {
      // Peer is done, close connection.
      m_shutdown = true;

    } else {
      m_sendInterested = m_up->get_interested();
      m_up->set_interested(false);
    }

  } else if (m_up->get_interested() &&
	     !m_download->delegator()->get_select().interested(m_bitfield.get_bitfield())) {
    // TODO: Optimize?
    m_sendInterested = true;
    m_up->set_interested(false);
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

  if (!m_download->content()->is_valid_piece(*p) || !m_bitfield[p->get_index()])
    throw internal_error("PeerConnection::send_request_piece() tried to use an invalid piece");

  m_up->write_request(*p);

  return true;
}

void
PeerConnection::receive_have(uint32_t index) {
  if (index >= m_bitfield.size_bits())
    throw communication_error("Recieved HAVE command with invalid value");

  if (m_bitfield[index])
    return;

  m_bitfield.set(index, true);
  m_download->get_bitfield_counter().inc(index);

  m_peerRate.insert(m_download->content()->get_chunk_size());
    
  if (m_download->content()->is_done())
    return;

  if (!m_up->get_interested() && m_download->delegator()->get_select().interested(index)) {
    m_sendInterested = true;
    m_up->set_interested(true);

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
    m_download->signal_network_log().emit("Received piece with length zero");
    m_down->set_state(ProtocolRead::IDLE);

    return;
  }

  if (m_requestList.downloading(p)) {
    m_down->set_state(ProtocolRead::READ_PIECE);
    m_down->set_position(0);

    load_down_chunk(p);
  } else {
    // We don't want this piece,
    m_down->set_state(ProtocolRead::SKIP_PIECE);
    m_down->set_length(p.get_length());
    m_down->set_position(0);
  }
}  

// Returns false for connections that should be closed.
bool
PeerConnection::receive_keepalive() {
  // Check if remote peer is dead.
  if (Timer::cache() - m_timeLastRead > 240 * 1000000) {
    //m_download->connection_list()->erase(this);
    return false;
  }

  // Send keep-alive only if we arn't already transmitting. There's no
  // point to send it if there is data being sendt, and we don't
  // bother with a timer for the last sendt message since it is
  // cleaner just to send this message.

  if (m_up->get_state() == ProtocolWrite::IDLE) {
    m_up->get_buffer().reset_position();
    m_up->write_keepalive();

    m_up->get_buffer().prepare_end();
    m_up->set_state(ProtocolWrite::MSG);

    pollCustom->insert_write(this);
  }

  m_tryRequest = true;

  // Stall pieces when more than one receive_keepalive() has been
  // called while a single piece is downloading.
  //
  // m_downStall is decremented for every successfull download, so it
  // should stay at zero or one when downloading at an acceptable
  // speed. Thus only when m_downStall >= 2 is the download actually
  // stalling.
  if (!m_requestList.empty() && m_downStall++ > 0)
    m_requestList.stall();

  return true;
}

void
PeerConnection::task_send_choke() {
  m_sendChoked = true;

  pollCustom->insert_write(this);
}

}
