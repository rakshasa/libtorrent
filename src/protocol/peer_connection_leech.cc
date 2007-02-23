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

#include <cstring>
#include <sstream>

#include "data/chunk_list_node.h"
#include "download/choke_manager.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_info.h"
#include "download/download_main.h"

#include "peer_connection_leech.h"

namespace torrent {

PeerConnectionLeech::~PeerConnectionLeech() {
//   if (m_download != NULL && m_down->get_state() != ProtocolRead::READ_BITFIELD)
//     m_download->bitfield_counter().dec(m_peerChunks.bitfield()->bitfield());

//   priority_queue_erase(&taskScheduler, &m_taskSendChoke);
}

void
PeerConnectionLeech::initialize_custom() {
//   if (m_download->content()->chunks_completed() != 0) {
//     m_up->write_bitfield(m_download->file_list()->bitfield()->size_bytes());

//     m_up->buffer()->prepare_end();
//     m_up->set_position(0);
//     m_up->set_state(ProtocolWrite::WRITE_BITFIELD_HEADER);
//   }
}

void
PeerConnectionLeech::update_interested() {
// FIXME:   if (m_download->delegator()->get_select().interested(m_peerChunks.bitfield()->bitfield())) {

  // Consider just setting to interested without checking the
  // bitfield. The status might change by the time we get unchoked
  // anyway.

  if (true) {
    m_sendInterested = !m_down->interested();
    m_down->set_interested(true);
  } else {
    m_sendInterested = m_down->interested();
    m_down->set_interested(false);
  }

  m_peerChunks.download_cache()->clear();
}

bool
PeerConnectionLeech::receive_keepalive() {
  if (cachedTime - m_timeLastRead > rak::timer::from_seconds(240))
    return false;

  // There's no point in adding ourselves to the write poll if the
  // buffer is full, as that will already have been taken care of.
  if (m_up->get_state() == ProtocolWrite::IDLE &&
      m_up->can_write_keepalive()) {

    write_insert_poll_safe();

    ProtocolBuffer<512>::iterator old_end = m_up->buffer()->end();
    m_up->write_keepalive();
    if (is_encrypted())
      m_encryption.encrypt(old_end, m_up->buffer()->end() - old_end);
  }

  m_tryRequest = true;

  // Stall pieces when more than one receive_keepalive() has been
  // called while a single piece is downloading.
  //
  // m_downStall is decremented for every successfull download, so it
  // should stay at zero or one when downloading at an acceptable
  // speed. Thus only when m_downStall >= 2 is the download actually
  // stalling.
  if (!download_queue()->empty() && m_downStall++ != 0)
    download_queue()->stall();

  return true;
}

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
inline bool
PeerConnectionLeech::read_message() {
  ProtocolBuffer<512>* buf = m_down->buffer();

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
    throw communication_error("PeerConnectionLeech::read_message() got an invalid message length.");
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

    m_peerChunks.download_cache()->disable();

    download_queue()->cancel();
    m_download->download_throttle()->erase(m_peerChunks.download_throttle());

    return true;

  case ProtocolBase::UNCHOKE:
    if (is_down_choked()) {
      write_insert_poll_safe();

      m_down->set_choked(false);
      m_tryRequest = true;
    }

    return true;

  case ProtocolBase::INTERESTED:
    m_download->upload_choke_manager()->set_interested(this);
    return true;

  case ProtocolBase::NOT_INTERESTED:
    m_download->upload_choke_manager()->set_not_interested(this);
    return true;

  case ProtocolBase::HAVE:
    if (!m_down->can_read_have_body())
      break;

    read_have_chunk(buf->read_32());
    return true;

  case ProtocolBase::REQUEST:
    if (!m_down->can_read_request_body())
      break;

    if (!m_up->choked()) {
      write_insert_poll_safe();
      read_request_piece(m_down->read_request());

    } else {
      m_down->read_request();
    }

    return true;

  case ProtocolBase::PIECE:
    if (!m_down->can_read_piece_body())
      break;

    if (!down_chunk_start(m_down->read_piece(length - 9))) {

      // We don't want this chunk.
      if (down_chunk_skip_from_buffer()) {
        m_tryRequest = true;
        down_chunk_finished();
        return true;

      } else {
        m_down->set_state(ProtocolRead::READ_SKIP_PIECE);
        return false;
      }
      
    } else {

      if (down_chunk_from_buffer()) {
        m_tryRequest = true;
        down_chunk_finished();
        return true;

      } else {
        m_down->set_state(ProtocolRead::READ_PIECE);
        m_download->download_throttle()->insert(m_peerChunks.download_throttle());
        return false;
      }
    }

  case ProtocolBase::CANCEL:
    if (!m_down->can_read_cancel_body())
      break;

    read_cancel_piece(m_down->read_request());
    return true;

  default:
    throw communication_error("Received unsupported message type.");
  }

  // We were unsuccessfull in reading the message, need more data.
  buf->set_position_itr(beginning);
  return false;
}

void
PeerConnectionLeech::event_read() {
  m_timeLastRead = cachedTime;

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
        if (m_down->buffer()->size_end() < read_size) {
          unsigned int length = read_stream_throws(m_down->buffer()->end(), read_size - m_down->buffer()->size_end());

          if (is_encrypted())
            m_encryption.decrypt(m_down->buffer()->end(), length);

          m_down->buffer()->move_end(length);
        }

        while (read_message());
        
        if (m_down->buffer()->size_end() == read_size) {
          m_down->buffer()->move_unused();
          break;
        } else {
          m_down->buffer()->move_unused();
          return;
        }

      case ProtocolRead::READ_PIECE:
        if (!download_queue()->is_downloading())
          throw internal_error("ProtocolRead::READ_PIECE state but RequestList is not downloading.");

        if (!m_downloadQueue.transfer()->is_valid() || !m_downloadQueue.transfer()->is_leader()) {
          m_down->set_state(ProtocolRead::READ_SKIP_PIECE);
          break;
        }

        if (!down_chunk())
          return;

        m_tryRequest = true;
        m_down->set_state(ProtocolRead::IDLE);
        down_chunk_finished();
        break;

      case ProtocolRead::READ_SKIP_PIECE:
        if (download_queue()->transfer()->is_leader()) {
          m_down->set_state(ProtocolRead::READ_PIECE);
          m_download->download_throttle()->insert(m_peerChunks.download_throttle());
          break;
        }

        if (!down_chunk_skip())
          return;

        m_tryRequest = true;
        m_down->set_state(ProtocolRead::IDLE);
        down_chunk_finished();
        break;

      default:
        throw internal_error("PeerConnectionLeech::event_read() wrong state.");
      }

      // Figure out how to get rid of the shouldLoop boolean.
    } while (true);

  // Exception handlers:

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (blocked_connection& e) {
    m_download->info()->signal_network_log().emit("Momentarily blocked read connection.");
    m_download->connection_list()->erase(this, 0);

  } catch (network_error& e) {
    m_download->info()->signal_network_log().emit(e.what());

    m_download->connection_list()->erase(this, 0);

  } catch (storage_error& e) {
    m_download->info()->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this, 0);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ',' << m_down->get_state() << ',' << m_down->last_command() << ") \"" << e.what() << '"';

    throw internal_error(s.str());
  }
}

inline void
PeerConnectionLeech::fill_write_buffer() {
  ProtocolBuffer<512>::iterator old_end = m_up->buffer()->end();

  // No need to use delayed choke as we are a leecher.
  if (m_sendChoked && m_up->can_write_choke()) {
    m_sendChoked = false;
    m_up->write_choke(m_up->choked());

    if (m_up->choked()) {
      m_download->upload_throttle()->erase(m_peerChunks.upload_throttle());
      up_chunk_release();
      m_peerChunks.upload_queue()->clear();

      if (m_encryptBuffer != NULL) {
        if (m_encryptBuffer->remaining())
          throw internal_error("Deleting encryptBuffer with encrypted data remaining.");

        delete m_encryptBuffer;
        m_encryptBuffer = NULL;
      }

    } else {
      m_download->upload_throttle()->insert(m_peerChunks.upload_throttle());
    }
  }

  // Send the interested state before any requests as some clients,
  // e.g. BitTornado 0.7.14 and uTorrent 0.3.0, disconnect if a
  // request has been received while uninterested. The problem arises
  // as they send unchoke before receiving interested.
  if (m_sendInterested && m_up->can_write_interested()) {
    m_up->write_interested(m_down->interested());
    m_sendInterested = false;
  }

  if (m_tryRequest &&

      !(m_tryRequest = !should_request()) &&
      !(m_tryRequest = try_request_pieces()) &&

      !download_queue()->is_interested_in_active()) {
    m_sendInterested = true;
    m_down->set_interested(false);
  }

  DownloadMain::have_queue_type* haveQueue = m_download->have_queue();

  if (!haveQueue->empty() &&
      m_peerChunks.have_timer() <= haveQueue->front().first &&
      m_up->can_write_have()) {
    DownloadMain::have_queue_type::iterator last = std::find_if(haveQueue->begin(), haveQueue->end(),
                                                                rak::greater(m_peerChunks.have_timer(), rak::mem_ref(&DownloadMain::have_queue_type::value_type::first)));

    do {
      m_up->write_have((--last)->second);
    } while (last != haveQueue->begin() && m_up->can_write_have());

    m_peerChunks.set_have_timer(last->first + 1);
  }

  while (!m_peerChunks.cancel_queue()->empty() && m_up->can_write_cancel()) {
    m_up->write_cancel(m_peerChunks.cancel_queue()->front());
    m_peerChunks.cancel_queue()->pop_front();
  }

  if (!m_up->choked() &&
      !m_peerChunks.upload_queue()->empty() &&
      m_up->can_write_piece())
    write_prepare_piece();

  if (is_encrypted())
    m_encryption.encrypt(old_end, m_up->buffer()->end() - old_end);
}

void
PeerConnectionLeech::event_write() {
  try {
  
    do {

      switch (m_up->get_state()) {
      case ProtocolWrite::IDLE:

        fill_write_buffer();

        if (m_up->buffer()->remaining() == 0) {
          manager->poll()->remove_write(this);
          return;
        }

        m_up->set_state(ProtocolWrite::MSG);

      case ProtocolWrite::MSG:
        if (!m_up->buffer()->consume(write_stream_throws(m_up->buffer()->position(), m_up->buffer()->remaining())))
          return;

        m_up->buffer()->reset();

        if (m_up->last_command() != ProtocolBase::PIECE) {
          // Break or loop? Might do an ifelse based on size of the
          // write buffer. Also the write buffer is relatively large.
          m_up->set_state(ProtocolWrite::IDLE);
          break;
        }

        // We're uploading a piece.
        load_up_chunk();

        m_up->set_state(ProtocolWrite::WRITE_PIECE);

      case ProtocolWrite::WRITE_PIECE:
        if (!up_chunk())
          return;

        m_up->set_state(ProtocolWrite::IDLE);

        break;

      default:
        throw internal_error("PeerConnectionLeech::event_write() wrong state.");
      }

    } while (true);

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (blocked_connection& e) {
    m_download->info()->signal_network_log().emit("Momentarily blocked write connection.");
    m_download->connection_list()->erase(this, 0);

  } catch (network_error& e) {
    m_download->info()->signal_network_log().emit(e.what());
    m_download->connection_list()->erase(this, 0);

  } catch (storage_error& e) {
    m_download->info()->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this, 0);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ',' << m_up->get_state() << ',' << m_up->last_command() << ") \"" << e.what() << '"';

    throw internal_error(s.str());
  }
}

void
PeerConnectionLeech::read_have_chunk(uint32_t index) {
  if (index >= m_peerChunks.bitfield()->size_bits())
    throw communication_error("Peer sent HAVE message with out-of-range index.");

  if (m_peerChunks.bitfield()->get(index))
    return;

  m_download->chunk_statistics()->received_have_chunk(&m_peerChunks, index, m_download->file_list()->chunk_size());

  if (m_peerChunks.bitfield()->is_all_set())
    if (m_download->file_list()->is_done())
      throw close_connection();
    else
      m_download->upload_choke_manager()->set_not_interested(this);

  if (m_download->file_list()->is_done())
    return;

  if (is_down_interested()) {

    if (!m_tryRequest && m_download->chunk_selector()->received_have_chunk(&m_peerChunks, index)) {
      m_tryRequest = true;
      write_insert_poll_safe();
    }

  } else {

    if (m_download->chunk_selector()->received_have_chunk(&m_peerChunks, index)) {
      m_sendInterested = true;
      m_down->set_interested(true);
      
      // Is it enough to insert into write here? Make the interested
      // check branch to include insert_write, even when not sending
      // interested.
      m_tryRequest = true;
      write_insert_poll_safe();
    }
  }
}

}
