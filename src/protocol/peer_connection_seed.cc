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

#include "peer_connection_seed.h"
#include "protocol_read.h"
#include "protocol_write.h"

namespace torrent {

PeerConnectionSeed::PeerConnectionSeed() {
}

PeerConnectionSeed::~PeerConnectionSeed() {
  if (!get_fd().is_valid())
    return;

//   if (m_down->get_state() != ProtocolRead::READ_BITFIELD)
//     m_download->get_bitfield_counter().dec(m_bitfield.get_bitfield());

//   taskScheduler.erase(&m_taskSendChoke);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);
  
  socketManager.close(get_fd());
  get_fd().clear();
}

void
PeerConnectionSeed::set(SocketFd fd, const PeerInfo& p, DownloadMain* download) {
  if (get_fd().is_valid())
    throw internal_error("Tried to re-set PeerConnection");

  set_fd(fd);

  m_peer = p;
  m_download = download;

  get_fd().set_throughput();

  m_requestList.set_delegator(m_download->delegator());
  m_requestList.set_bitfield(&m_bitfield.get_bitfield());

  if (m_download == NULL || !p.is_valid() || !get_fd().is_valid())
    throw internal_error("PeerConnectionSeed::set(...) recived bad input.");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(m_download->content()->get_chunk_total());

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  m_up->get_buffer().reset();
  m_down->get_buffer().reset();

  m_down->set_state(ProtocolRead::IDLE);
  m_up->set_state(ProtocolWrite::IDLE);

  if (m_download->content()->get_chunks_completed() != 0) {
    m_up->write_bitfield(m_download->content()->get_bitfield().size_bytes());

    m_up->get_buffer().prepare_end();
    m_up->set_position(0);
    m_up->set_state(ProtocolWrite::WRITE_BITFIELD_HEADER);
  }
    
  m_timeLastRead = Timer::cache();
}

void
PeerConnectionSeed::set_choke(bool v) {
  if (v == m_up->get_choked())
    return;

  m_up->set_choked(v);
  m_sendChoked = true;

  pollCustom->insert_write(this);
}

void
PeerConnectionSeed::update_interested() {
}

void
PeerConnectionSeed::receive_have_chunk(int32_t i) {
}

bool
PeerConnectionSeed::receive_keepalive() {
  if (Timer::cache() - m_timeLastRead > 240 * 1000000)
    return false;

  if (m_up->get_state() == ProtocolWrite::IDLE) {
    // Remove this check if ever we add other tasks that inject
    // messages into the buffer.
    if (m_up->get_buffer().size_position() != 0)
      throw internal_error("PeerConnectionSeed::receive_keepalive() called but m_up->get_buffer().size_position() != 0.");

    m_up->write_keepalive();
    pollCustom->insert_write(this);
  }

  return true;
}

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
inline bool
PeerConnectionSeed::read_message() {
  ProtocolBuffer<512>* buf = &m_down->get_buffer();

  if (buf->remaining() < 4)
    return false;

  // Remember the start of the message so we may reset it if we don't
  // have the whole message.
  ProtocolBuffer<512>::iterator beginning = buf->position();

  uint32_t length = buf->read_32();

  if (length == 0) {
    // Keepalive message.
    return true;

  } else if (buf->remaining() < 1) {
    buf->set_position_itr(beginning);
    return false;

  } else if (length > (1 << 20)) {
    throw network_error("PeerConnectionSeed::read_message() got an invalid message length.");
  }
    
  // We do not verify the message length of those with static
  // length. A bug in the remote client causing the message start to
  // be unsyncronized would in practically all cases be caught with
  // the above test.
  //
  // Those that do in some weird way manage to produce a valid
  // command, will not be able to do any damage as malicious
  // peer. Those cases should be caught elsewhere in the code.

  switch (buf->read_8()) {
  case ProtocolBase::CHOKE:
    m_down->set_choked(true);
    return true;

  case ProtocolBase::UNCHOKE:
    m_down->set_choked(false);
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
    if (length != m_bitfield.size_bytes() + 1)
      throw network_error("Received invalid bitfield size.");

    std::memcpy(m_bitfield.begin(), buf->position(), std::min<uint32_t>(buf->remaining(), m_bitfield.size_bytes()));

    if (buf->remaining() >= m_bitfield.size_bytes()) {
      buf->move_position(m_bitfield.size_bytes());
      finish_bitfield();

      return true;

    } else {
      m_down->set_position(buf->remaining());
      m_down->set_state(ProtocolRead::READ_BITFIELD);

      // Move the position so we don't copy 'unread' bytes to the
      // start of the buffer.
      buf->move_position(buf->remaining());

      return false;
    }

  case ProtocolBase::REQUEST:
    if (buf->remaining() < 13)
      break;

    if (!m_up->get_choked()) {
      read_request_piece(m_down->read_request());
      pollCustom->insert_write(this);

    } else {
      m_down->read_piece();
    }

    return true;

  case ProtocolBase::PIECE:
    throw network_error("Received a piece but the connection is strictly for seeding.");

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
PeerConnectionSeed::event_read() {
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

    bool shouldLoop;

    do {

      switch (m_down->get_state()) {
      case ProtocolRead::IDLE:
	m_down->get_buffer().move_end(read_buf(m_down->get_buffer().end(), read_size - m_down->get_buffer().size_end()));
	
	while (read_message());
	
	shouldLoop = m_down->get_buffer().size_end() == read_size;
	read_buffer_move_unused();

	break;

      case ProtocolRead::READ_BITFIELD:
	// We're guaranteed that we still got bytes remaining to be
	// read of the bitfield.
	m_down->adjust_position(read_buf(m_bitfield.begin() + m_down->get_position(),
					 m_bitfield.size_bytes() - m_down->get_position()));
	
	if (m_down->get_position() != m_bitfield.size_bytes())
	  return;

	m_down->set_state(ProtocolRead::IDLE);
	m_down->get_buffer().reset();

	finish_bitfield();

	shouldLoop = true;
	break;

      default:
	throw internal_error("PeerConnectionSeed::event_read() wrong state.");
      }

      // Figure out how to get rid of the shouldLoop boolean.
    } while (shouldLoop);

  // Exception handlers:

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

inline void
PeerConnectionSeed::fill_write_buffer() {
  // No need to use delayed choke as we are a seeder.
  if (m_sendChoked) {
    m_sendChoked = false;

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
  }

  if (!m_up->get_choked() &&
      !m_sendList.empty() &&
      m_up->can_write_piece())
    write_prepare_piece();
}

void
PeerConnectionSeed::event_write() {
  try {
  
    do {

      switch (m_up->get_state()) {
      case ProtocolWrite::IDLE:

	// We might have buffered keepalive message or similar, but
	// 'end' should remain at the start of the buffer.
	if (m_up->get_buffer().size_end() != 0)
	  throw internal_error("PeerConnectionSeed::event_write() ProtocolWrite::IDLE in a wrong state.");

	// Fill up buffer.
	fill_write_buffer();

	if (m_up->get_buffer().size_position() == 0) {
	  pollCustom->remove_write(this);
	  return;
	}

	m_up->set_state(ProtocolWrite::MSG);
	m_up->get_buffer().prepare_end();

      case ProtocolWrite::MSG:
	m_up->get_buffer().move_position(write_buf(m_up->get_buffer().position(), m_up->get_buffer().remaining()));

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

	if (m_sendList.empty() || m_sendList.front() != m_upPiece)
	  throw internal_error("ProtocolWrite::WRITE_PIECE found the wrong piece in the send queue.");

	// Do we need to check that this is the right piece?
	m_sendList.pop_front();
	
	if (m_sendList.empty()) {
	  m_download->content()->release_chunk(m_upChunk);
	  m_upChunk = NULL;
	}
	
	m_up->set_state(ProtocolWrite::IDLE);
	break;

      case ProtocolWrite::WRITE_BITFIELD_HEADER:
	m_up->get_buffer().move_position(write_buf(m_up->get_buffer().position(), m_up->get_buffer().remaining()));

	if (m_up->get_buffer().remaining())
	  return;

	m_up->get_buffer().reset();
	m_up->set_state(ProtocolWrite::WRITE_BITFIELD);

      case ProtocolWrite::WRITE_BITFIELD:
	m_up->adjust_position(write_buf(m_download->content()->get_bitfield().begin() + m_up->get_position(),
					m_download->content()->get_bitfield().size_bytes() - m_up->get_position()));

	if (m_up->get_position() != m_bitfield.size_bytes())
	  return;

	m_up->set_state(ProtocolWrite::IDLE);
	break;

      default:
	throw internal_error("PeerConnectionSeed::event_write() wrong state.");
      }

    } while (true);

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

void
PeerConnectionSeed::event_error() {
  uint8_t oob;

  if (!read_oob(&oob)) {
    m_download->signal_network_log().emit("PeerConnectionSeed::event_error() received but could not read it.");
    return;
  }

  std::stringstream str;
  str << "PeerConnectionSeed::event_error() received: " << (uint32_t)oob << '.';

  m_download->signal_network_log().emit(str.str());
}

void
PeerConnectionSeed::read_have_chunk(uint32_t index) {
  if (index >= m_bitfield.size_bits())
    throw network_error("Peer sent HAVE message with out-of-range index.");

  m_bitfield.set(index, true);
  m_peerRate.insert(m_download->content()->get_chunk_size());
}

void
PeerConnectionSeed::finish_bitfield() {
  m_bitfield.update_count();

  if (m_bitfield.all_set())
    throw close_connection();

//   m_download->get_bitfield_counter().inc(m_bitfield.get_bitfield());

  pollCustom->insert_write(this);
}

}
