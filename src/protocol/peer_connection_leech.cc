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

#include <cstring>
#include <sstream>

#include "data/content.h"
#include "download/download_main.h"

#include "peer_connection_leech.h"

namespace torrent {

PeerConnectionLeech::~PeerConnectionLeech() {
//   if (m_download != NULL && m_down->get_state() != ProtocolRead::READ_BITFIELD)
//     m_download->get_bitfield_counter().dec(m_bitfield.get_bitfield());

//   taskScheduler.erase(&m_taskSendChoke);
}

void
PeerConnectionLeech::initialize_custom() {
  if (m_download->content()->get_chunks_completed() != 0) {
    m_up->write_bitfield(m_download->content()->get_bitfield().size_bytes());

    m_up->get_buffer().prepare_end();
    m_up->set_position(0);
    m_up->set_state(ProtocolWrite::WRITE_BITFIELD_HEADER);
  }
}

void
PeerConnectionLeech::set_choke(bool v) {
  if (v == m_up->get_choked())
    return;

  m_up->set_choked(v);
  m_sendChoked = true;

  pollCustom->insert_write(this);
}

void
PeerConnectionLeech::update_interested() {
  if (m_download->delegator()->get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_down->get_interested();
    m_down->set_interested(true);
  } else {
    m_sendInterested = m_down->get_interested();
    m_down->set_interested(false);
  }
}

void
PeerConnectionLeech::receive_have_chunk(int32_t index) {
  m_haveQueue.push_back(index);

  if (m_requestList.has_index(index))
    throw internal_error("PeerConnection::sendHave(...) found a request with the same index");
}

bool
PeerConnectionLeech::receive_keepalive() {
  if (Timer::cache() - m_timeLastRead > 240 * 1000000)
    return false;

  // There's no point in adding ourselves to the write poll if the
  // buffer is full, as that will already have been taken care of.
  if (m_up->get_state() == ProtocolWrite::IDLE &&
      m_up->can_write_keepalive()) {

    m_up->write_keepalive();
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

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
inline bool
PeerConnectionLeech::read_message() {
  ProtocolBuffer<512>* buf = &m_down->get_buffer();

  if (buf->remaining() < 4)
    return false;

  // Remember the start of the message so we may reset it if we don't
  // have the whole message.
  ProtocolBuffer<512>::iterator beginning = buf->position();

  uint32_t length = buf->read_32();

  if (length == 0) {
    // Keepalive message.
    m_down->set_last_command(ProtocolBase::KEEP_ALIVE);

    return true;

  } else if (buf->remaining() < 1) {
    buf->set_position_itr(beginning);
    return false;

  } else if (length > (1 << 20)) {
    throw network_error("PeerConnectionLeech::read_message() got an invalid message length.");
  }
    
  // We do not verify the message length of those with static
  // length. A bug in the remote client causing the message start to
  // be unsyncronized would in practically all cases be caught with
  // the above test.
  //
  // Those that do in some weird way manage to produce a valid
  // command, will not be able to do any damage as malicious
  // peer. Those cases should be caught elsewhere in the code.

  // Temporary.
  m_down->set_last_command((ProtocolBase::Protocol)buf->peek_8());

  switch (buf->read_8()) {
  case ProtocolBase::CHOKE:
    m_down->set_choked(true);

    m_requestList.cancel();
    remove_down_throttle();

    return true;

  case ProtocolBase::UNCHOKE:
    if (is_down_choked()) {
      m_down->set_choked(false);
      m_tryRequest = true;

      pollCustom->insert_write(this);
    }

    return true;

  case ProtocolBase::INTERESTED:
    if (m_up->get_choked() && m_download->size_unchoked() < m_download->choke_manager()->get_max_unchoked())
      set_choke(false);

    m_down->set_interested(true);
    return true;

  case ProtocolBase::NOT_INTERESTED:
    if (!m_up->get_choked()) {
      set_choke(true);
      m_download->choke_balance();
    }

    m_down->set_interested(false);
    return true;

  case ProtocolBase::HAVE:
    if (buf->remaining() < 4)
      break;

    read_have_chunk(buf->read_32());
    return true;

  case ProtocolBase::BITFIELD:
    if (read_bitfield_from_buffer(length - 1)) {
      finish_bitfield();
      return true;

    } else {
      m_down->set_state(ProtocolRead::READ_BITFIELD);
      return false;
    }

  case ProtocolBase::REQUEST:
    if (buf->remaining() < 13)
      break;

    if (!m_up->get_choked()) {
      read_request_piece(m_down->read_request());
      pollCustom->insert_write(this);

    } else {
      m_down->read_request();
    }

    return true;

  case ProtocolBase::PIECE:
    if (buf->remaining() < 9)
      break;

    m_down->set_position(0);
    m_downPiece = m_down->read_piece(length - 9);

    if (!receive_piece_header()) {

      // We're skipping this piece.
      m_down->set_position(std::min<uint32_t>(buf->remaining(), m_downPiece.get_length()));
      buf->move_position(m_down->get_position());

      if (m_down->get_position() != m_downPiece.get_length()) {
	m_down->set_state(ProtocolRead::READ_SKIP_PIECE);
	return false;

      } else {
	return true;
      }
      
    } else if (down_chunk_from_buffer()) {
      // Done with chunk.
      m_downChunk->set_time_modified(Timer::cache());
      m_requestList.finished();
    
      if (m_downStall > 0)
	m_downStall--;
    
      // TODO: clear m_down.data?
      // TODO: remove throttle if choked? Rarely happens though.
      m_tryRequest = true;
      pollCustom->insert_write(this);

      return true;

    } else {
      m_down->set_state(ProtocolRead::READ_PIECE);
      insert_down_throttle();
      return false;
    }

  case ProtocolBase::CANCEL:
    if (buf->remaining() < 13)
      break;

    read_cancel_piece(m_down->read_request());
    return true;

  default:
    throw network_error("Received unsupported message type.");
  }

  // We were unsuccessfull in reading the message, need more data.
  buf->set_position_itr(beginning);
  return false;
}

void
PeerConnectionLeech::event_read() {
  m_timeLastRead = Timer::cache();

  // Need to make sure ProtocolBuffer::end() is pointing to the end of
  // the unread data, and that the unread data starts from the
  // beginning of the buffer. Or do we use position? Propably best,
  // therefor ProtocolBuffer::position() points to the beginning of
  // the unused data.

  try {
    
    // Normal read.
    //
    // We rarely will read zero bytes as the read of 64 bytes will
    // almost always either not fill up or it will require additional
    // reads.
    //
    // Only loop when end hits 64.

    do {

      switch (m_down->get_state()) {
      case ProtocolRead::IDLE:
	m_down->get_buffer().move_end(read_stream_throws(m_down->get_buffer().end(), read_size - m_down->get_buffer().size_end()));
	
	while (read_message());
	
	if (m_down->get_buffer().size_end() == read_size) {
	  read_buffer_move_unused();
	  break;
	} else {
	  read_buffer_move_unused();
	  return;
	}

      case ProtocolRead::READ_PIECE:
	if (!m_requestList.is_downloading())
	  throw internal_error("ProtocolRead::READ_PIECE state but RequestList is not downloading");

	if (!m_requestList.is_wanted()) {
	  m_down->set_state(ProtocolRead::READ_SKIP_PIECE);
	  m_requestList.skip();

	  break;
	}

	if (!down_chunk())
	  return;

	m_downChunk->set_time_modified(Timer::cache());
	m_requestList.finished();
	
	if (m_downStall > 0)
	  m_downStall--;
	
	// TODO: clear m_down.data?
	// TODO: remove throttle if choked? Rarely happens though.
	m_tryRequest = true;
	m_down->set_state(ProtocolRead::IDLE);
	pollCustom->insert_write(this);
	
	break;

      case ProtocolRead::READ_SKIP_PIECE:
	m_down->adjust_position(ignore_stream_throws(m_downPiece.get_length() - m_down->get_position()));

	if (m_down->get_position() != m_downPiece.get_length())
	  return;

	m_down->set_state(ProtocolRead::IDLE);
	m_down->get_buffer().reset();
	
	break;

      case ProtocolRead::READ_BITFIELD:
	if (!read_bitfield_body())
	  return;

	m_down->set_state(ProtocolRead::IDLE);
	m_down->get_buffer().reset();

	finish_bitfield();
	break;

      default:
	throw internal_error("PeerConnectionLeech::event_read() wrong state.");
      }

      // Figure out how to get rid of the shouldLoop boolean.
    } while (true);

  // Exception handlers:

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (blocked_connection& e) {
    m_download->signal_network_log().emit("Momentarily blocked read connection.");
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->signal_network_log().emit(e.what());

    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ',' << m_down->get_state() << ',' << m_down->get_last_command() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

inline void
PeerConnectionLeech::fill_write_buffer() {
  // No need to use delayed choke as we are a leecher.
  if (m_sendChoked) {
    m_sendChoked = false;

    m_up->write_choke(m_up->get_choked());
    m_lastChoked = Timer::cache();

    if (m_up->get_choked()) {
      remove_up_throttle();
      up_chunk_release();
	
      m_sendList.clear();

    } else {
      insert_up_throttle();
    }
  }

  if (m_tryRequest &&

      !(m_tryRequest = !should_request()) &&
      !(m_tryRequest = try_request_pieces()) &&

      !m_requestList.is_interested_in_active()) {
    m_sendInterested = true;
    m_up->set_interested(false);
  }

  if (m_sendInterested) {
    m_up->write_interested(m_up->get_interested());
    m_sendInterested = false;
  }

  while (!m_haveQueue.empty() && m_up->can_write_have()) {
    m_up->write_have(m_haveQueue.front());
    m_haveQueue.pop_front();
  }

  if (!m_up->get_choked() &&
      !m_sendList.empty() &&
      m_up->can_write_piece())
    write_prepare_piece();
}

void
PeerConnectionLeech::event_write() {
  try {
  
    do {

      switch (m_up->get_state()) {
      case ProtocolWrite::IDLE:

	// We might have buffered keepalive message or similar, but
	// 'end' should remain at the start of the buffer.
	if (m_up->get_buffer().size_end() != 0)
	  throw internal_error("PeerConnectionLeech::event_write() ProtocolWrite::IDLE in a wrong state.");

	// Fill up buffer.
	fill_write_buffer();

	if (m_up->get_buffer().size_position() == 0) {
	  pollCustom->remove_write(this);
	  return;
	}

	m_up->set_state(ProtocolWrite::MSG);
	m_up->get_buffer().prepare_end();

      case ProtocolWrite::MSG:
	m_up->get_buffer().move_position(write_stream_throws(m_up->get_buffer().position(), m_up->get_buffer().remaining()));

	if (m_up->get_buffer().remaining())
	  return;

	m_up->get_buffer().reset();

	if (m_up->get_last_command() != ProtocolBase::PIECE) {
	  // Break or loop? Might do an ifelse based on size of the
	  // write buffer. Also the write buffer is relatively large.
	  m_up->set_state(ProtocolWrite::IDLE);
	  break;
	}

	// We're uploading a piece.
	load_up_chunk();

	m_up->set_state(ProtocolWrite::WRITE_PIECE);
	m_up->set_position(0);

      case ProtocolWrite::WRITE_PIECE:
	if (!up_chunk())
	  return;

	write_finished_piece();
	m_up->set_state(ProtocolWrite::IDLE);

	break;

      case ProtocolWrite::WRITE_BITFIELD_HEADER:
	m_up->get_buffer().move_position(write_stream_throws(m_up->get_buffer().position(), m_up->get_buffer().remaining()));

	if (m_up->get_buffer().remaining())
	  return;

	m_up->get_buffer().reset();
	m_up->set_state(ProtocolWrite::WRITE_BITFIELD);

      case ProtocolWrite::WRITE_BITFIELD:
	if (!write_bitfield_body())
	  return;

	m_up->set_state(ProtocolWrite::IDLE);
	break;

      default:
	throw internal_error("PeerConnectionLeech::event_write() wrong state.");
      }

    } while (true);

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (blocked_connection& e) {
    m_download->signal_network_log().emit("Momentarily blocked write connection.");
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->signal_network_log().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ',' << m_up->get_state() << ',' << m_up->get_last_command() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void
PeerConnectionLeech::read_have_chunk(uint32_t index) {
  if (index >= m_bitfield.size_bits())
    throw network_error("Peer sent HAVE message with out-of-range index.");

  m_bitfield.set(index, true);
  m_peerRate.insert(m_download->content()->chunk_size());
}

void
PeerConnectionLeech::finish_bitfield() {
  m_bitfield.update_count();

  // Check if we're done. Should we just put ourselves in interested
  // state from the beginning? We should then make sure we send not
  // interested, we already do?
  if (!m_download->content()->is_done()) {
    m_tryRequest = true;
    m_sendInterested = true;
    m_up->set_interested(true);
    
  } else if (m_bitfield.all_set()) {
    throw close_connection();
  }

//   m_download->get_bitfield_counter().inc(m_bitfield.get_bitfield());

  pollCustom->insert_write(this);
}

bool
PeerConnectionLeech::receive_piece_header() {
  if (m_requestList.downloading(m_downPiece)) {

    load_down_chunk(m_downPiece);
    return true;

  } else {
    if (m_downPiece.get_length() == 0)
      m_download->signal_network_log().emit("Received piece with length zero");

    return false;
  }
}

}
