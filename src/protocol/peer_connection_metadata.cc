// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_main.h"
#include "torrent/dht_manager.h"
#include "torrent/download_info.h"
#include "torrent/download/choke_queue.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer_info.h"
#include "rak/functional.h"
#include "torrent/utils/log.h"

#include "extensions.h"
#include "peer_connection_metadata.h"

#define LT_LOG_METADATA_EVENTS(log_fmt, ...)                            \
  lt_log_print_info(LOG_PROTOCOL_METADATA_EVENTS, this->download()->info(), "metadata_events", "%40s " log_fmt, this->peer_info()->id_hex(), __VA_ARGS__);
#define LT_LOG_STORAGE_ERRORS(log_fmt, ...)                              \
  lt_log_print_info(LOG_PROTOCOL_STORAGE_ERRORS, this->download()->info(), "storage_errors", "%40s " log_fmt, this->peer_info()->id_hex(), __VA_ARGS__);

namespace torrent {

PeerConnectionMetadata::~PeerConnectionMetadata() {
}

void
PeerConnectionMetadata::initialize_custom() {
}

void
PeerConnectionMetadata::update_interested() {
}

bool
PeerConnectionMetadata::receive_keepalive() {
  if (cachedTime - m_timeLastRead > rak::timer::from_seconds(240))
    return false;

  m_tryRequest = true;

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

  return true;
}

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
inline bool
PeerConnectionMetadata::read_message() {
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
    throw communication_error("PeerConnection::read_message() got an invalid message length.");
  }
    
  m_down->set_last_command((ProtocolBase::Protocol)buf->peek_8());

  // Ignore most messages, they aren't relevant for a metadata download.
  switch (buf->read_8()) {
  case ProtocolBase::CHOKE:
  case ProtocolBase::UNCHOKE:
  case ProtocolBase::INTERESTED:
  case ProtocolBase::NOT_INTERESTED:
    return true;

  case ProtocolBase::HAVE:
    if (!m_down->can_read_have_body())
      break;

    buf->read_32();
    return true;

  case ProtocolBase::REQUEST:
    if (!m_down->can_read_request_body())
      break;

    m_down->read_request();
    return true;

  case ProtocolBase::PIECE:
    throw communication_error("Received a piece but the connection is strictly for meta data.");

  case ProtocolBase::CANCEL:
    if (!m_down->can_read_cancel_body())
      break;

    m_down->read_request();
    return true;

  case ProtocolBase::PORT:
    if (!m_down->can_read_port_body())
      break;

    manager->dht_manager()->add_node(m_peerInfo->socket_address(), m_down->buffer()->read_16());
    return true;

  case ProtocolBase::EXTENSION_PROTOCOL:
    LT_LOG_METADATA_EVENTS("protocol extension message", 0);

    if (!m_down->can_read_extension_body())
      break;

    if (m_extensions->is_default()) {
      m_extensions = new ProtocolExtension();
      m_extensions->set_info(m_peerInfo, m_download);
    }

    {
      int extension = m_down->buffer()->read_8();
      m_extensions->read_start(extension, length - 2, (extension == ProtocolExtension::UT_PEX) && !m_download->want_pex_msg());
      m_down->set_state(ProtocolRead::READ_EXTENSION);
    }

    if (!down_extension())
      return false;

    LT_LOG_METADATA_EVENTS("protocol extension done", 0);

    // Drop peer if it disabled the metadata extension.
    if (!m_extensions->is_remote_supported(ProtocolExtension::UT_METADATA))
      throw close_connection();

    m_down->set_state(ProtocolRead::IDLE);
    m_tryRequest = true;
    write_insert_poll_safe();

    return true;

  case ProtocolBase::BITFIELD:
    // Discard the bitfield sent by the peer.
    m_skipLength = length - 1;
    m_down->set_state(ProtocolRead::READ_SKIP_PIECE);
    return false;

  default:
    throw communication_error("Received unsupported message type.");
  }

  // We were unsuccessfull in reading the message, need more data.
  buf->set_position_itr(beginning);
  return false;
}

void
PeerConnectionMetadata::event_read() {
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
          m_down->throttle()->node_used_unthrottled(length);

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

      case ProtocolRead::READ_EXTENSION:
        if (!down_extension())
          return;

        // Drop peer if it disabled the metadata extension.
        if (!m_extensions->is_remote_supported(ProtocolExtension::UT_METADATA))
          throw close_connection();

        LT_LOG_METADATA_EVENTS("reading extension message", 0);

        m_down->set_state(ProtocolRead::IDLE);
        m_tryRequest = true;
        write_insert_poll_safe();
        break;

      // Actually skipping the bitfield.
      // We never receive normal piece messages anyway.
      case ProtocolRead::READ_SKIP_PIECE:
        if (!read_skip_bitfield())
          return;

        m_down->set_state(ProtocolRead::IDLE);
        break;

      default:
        throw internal_error("PeerConnection::event_read() wrong state.");
      }

      // Figure out how to get rid of the shouldLoop boolean.
    } while (true);

  // Exception handlers:

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (blocked_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (network_error& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (storage_error& e) {
    LT_LOG_STORAGE_ERRORS("read error: %s", e.what());
    m_download->connection_list()->erase(this, 0);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ',' << m_down->get_state() << ',' << m_down->last_command() << ") \"" << e.what() << '"';

    throw internal_error(s.str());
  }
}

inline void
PeerConnectionMetadata::fill_write_buffer() {
  ProtocolBuffer<512>::iterator old_end = m_up->buffer()->end();

  if (m_tryRequest)
    m_tryRequest = try_request_metadata_pieces();

  if (m_sendPEXMask && m_up->can_write_extension() &&
      send_pex_message()) {
    // Don't do anything else if send_pex_message() succeeded.

  } else if (m_extensions->has_pending_message() && m_up->can_write_extension() &&
             send_ext_message()) {
    // Same.
  }

  if (is_encrypted())
    m_encryption.encrypt(old_end, m_up->buffer()->end() - old_end);
}

void
PeerConnectionMetadata::event_write() {
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
        if (!m_up->buffer()->consume(m_up->throttle()->node_used_unthrottled(write_stream_throws(m_up->buffer()->position(),
                                                                                                 m_up->buffer()->remaining()))))
          return;

        m_up->buffer()->reset();

        if (m_up->last_command() != ProtocolBase::EXTENSION_PROTOCOL) {
          m_up->set_state(ProtocolWrite::IDLE);
          break;
        }

        m_up->set_state(ProtocolWrite::WRITE_EXTENSION);

      case ProtocolWrite::WRITE_EXTENSION:
        if (!up_extension())
          return;

        m_up->set_state(ProtocolWrite::IDLE);
        break;

      default:
        throw internal_error("PeerConnection::event_write() wrong state.");
      }

    } while (true);

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (blocked_connection& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (network_error& e) {
    m_download->connection_list()->erase(this, 0);

  } catch (storage_error& e) {
    LT_LOG_STORAGE_ERRORS("read error: %s", e.what());
    m_download->connection_list()->erase(this, 0);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ',' << m_up->get_state() << ',' << m_up->last_command() << ") \"" << e.what() << '"';

    throw internal_error(s.str());
  }
}

bool
PeerConnectionMetadata::read_skip_bitfield() {
  if (m_down->buffer()->remaining()) {
    uint32_t length = std::min(m_skipLength, (uint32_t)m_down->buffer()->remaining());
    m_down->buffer()->consume(length);
    m_skipLength -= length;
  }

  if (m_skipLength) {
    uint32_t length = std::min(m_skipLength, (uint32_t)null_buffer_size);
    length = read_stream_throws(m_nullBuffer, length);
    if (!length)
      return false;
    m_skipLength -= length;
  }

  return !m_skipLength;
}

// Same as the PCB code, but only one at a time and with the extension protocol.
bool
PeerConnectionMetadata::try_request_metadata_pieces() {
  if (m_download->file_list()->chunk_size() == 1 || !m_extensions->is_remote_supported(ProtocolExtension::UT_METADATA))
    return false;

  if (request_list()->queued_empty())
    m_downStall = 0;

  uint32_t pipeSize = request_list()->calculate_pipe_size(m_peerChunks.download_throttle()->rate()->rate());

  // Don't start requesting if we can't do it in large enough chunks.
  if (request_list()->pipe_size() >= (pipeSize + 10) / 2)
    return false;

  // DEBUG:
//   if (!request_list()->queued_size() < pipeSize || !m_up->can_write_extension() ||
  if (!m_up->can_write_extension() || m_extensions->has_pending_message())
    return false;

  const Piece* p = request_list()->delegate();

  if (p == NULL)
    return false;

  if (!m_download->file_list()->is_valid_piece(*p) || !m_peerChunks.bitfield()->get(p->index()))
    throw internal_error("PeerConnectionMetadata::try_request_metadata_pieces() tried to use an invalid piece.");

  // DEBUG:
  if (m_extensions->request_metadata_piece(p)) {
    LT_LOG_METADATA_EVENTS("request metadata piece succeded", 0);
    return true;
  } else {
    LT_LOG_METADATA_EVENTS("request metadata piece failed", 0);
    return false;
  }
}

void
PeerConnectionMetadata::receive_metadata_piece(uint32_t piece, const char* data, uint32_t length) {
  if (data == NULL) {
    // Length is not set in a reject message.
    length = ProtocolExtension::metadata_piece_size;

    if ((piece << ProtocolExtension::metadata_piece_shift) + ProtocolExtension::metadata_piece_size >= m_download->file_list()->size_bytes())
      length = m_download->file_list()->chunk_size() % ProtocolExtension::metadata_piece_size;

    m_tryRequest = false;
    read_cancel_piece(Piece(0, piece << ProtocolExtension::metadata_piece_shift, length));

    LT_LOG_METADATA_EVENTS("rejected metadata piece", 0);
    return;
  }

  if (!down_chunk_start(Piece(0, piece << ProtocolExtension::metadata_piece_shift, length))) {
    LT_LOG_METADATA_EVENTS("skipped metadata piece", 0);
    down_chunk_skip_process(data, length);
  } else {
    LT_LOG_METADATA_EVENTS("processed metadata piece", 0);
    down_chunk_process(data, length);
  }

  if (m_request_list.transfer() != NULL && !m_request_list.transfer()->is_finished())
    throw internal_error("PeerConnectionMetadata::receive_metadata_piece did not have complete piece.");

  m_tryRequest = true;
  down_chunk_finished();
}

}
