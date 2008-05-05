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

#include <cstring>
#include <sstream>

#include "data/chunk_list_node.h"
#include "download/choke_manager.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_info.h"
#include "download/download_main.h"
#include "torrent/dht_manager.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer_info.h"

#include "extensions.h"
#include "initial_seed.h"
#include "peer_connection_leech.h"

namespace torrent {

template<Download::ConnectionType type>
PeerConnection<type>::~PeerConnection() {
//   if (m_download != NULL && m_down->get_state() != ProtocolRead::READ_BITFIELD)
//     m_download->bitfield_counter().dec(m_peerChunks.bitfield()->bitfield());

//   priority_queue_erase(&taskScheduler, &m_taskSendChoke);
}

template<Download::ConnectionType type>
void
PeerConnection<type>::initialize_custom() {
  if (type == Download::CONNECTION_INITIAL_SEED) {
    if (m_download->initial_seeding() == NULL) {
      // Can't throw close_connection or network_error here, we're still
      // initializing. So close the socket and let that kill it later.
      get_fd().close();
      return;
    }

    m_download->initial_seeding()->new_peer(this);
  }
//   if (m_download->content()->chunks_completed() != 0) {
//     m_up->write_bitfield(m_download->file_list()->bitfield()->size_bytes());

//     m_up->buffer()->prepare_end();
//     m_up->set_position(0);
//     m_up->set_state(ProtocolWrite::WRITE_BITFIELD_HEADER);
//   }
}

template<Download::ConnectionType type>
void
PeerConnection<type>::update_interested() {
  if (type != Download::CONNECTION_LEECH)
    return;

  m_peerChunks.download_cache()->clear();

  if (m_downInterested)
    return;

  // Consider just setting to interested without checking the
  // bitfield. The status might change by the time we get unchoked
  // anyway.

  m_sendInterested = !m_downInterested;
  m_downInterested = true;

  // Hmm... does this belong here, or should we insert ourselves into
  // the queue when we receive the unchoke?
//   m_download->download_choke_manager()->set_queued(this, &m_downChoke);
}

template<Download::ConnectionType type>
bool
PeerConnection<type>::receive_keepalive() {
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

  if (type != Download::CONNECTION_LEECH)
    return true;

  m_tryRequest = true;

  // Stall pieces when more than one receive_keepalive() has been
  // called while a single piece is downloading.
  //
  // m_downStall is decremented for every successfull download, so it
  // should stay at zero or one when downloading at an acceptable
  // speed. Thus only when m_downStall >= 2 is the download actually
  // stalling.
  //
  // If more than 6 ticks have gone by, assume the peer forgot about
  // our requests or tried to cheat with empty piece messages, and try
  // again.

  // TODO: Do we need to remove from download throttle here?
  //
  // Do we also need to remove from download throttle? Check how it
  // worked again.

  if (!download_queue()->canceled_empty() && m_downStall >= 6)
    download_queue()->cancel();
  else if (!download_queue()->queued_empty() && m_downStall++ != 0)
    download_queue()->stall();

  return true;
}

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
template<Download::ConnectionType type>
inline bool
PeerConnection<type>::read_message() {
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
    
  // We do not verify the message length of those with static
  // length. A bug in the remote client causing the message start to
  // be unsyncronized would in practically all cases be caught with
  // the above test.
  //
  // Those that do in some weird way manage to produce a valid
  // command, will not be able to do any more damage than a malicious
  // peer. Those cases should be caught elsewhere in the code.

  // Temporary.
  m_down->set_last_command((ProtocolBase::Protocol)buf->peek_8());

  switch (buf->read_8()) {
  case ProtocolBase::CHOKE:
    if (type != Download::CONNECTION_LEECH)
      return true;

    // Cancel before dequeueing so receive_download_choke knows if it
    // should remove us from throttle.
    //
    // Hmm... that won't work, as we arn't necessarily unchoked when
    // in throttle.

    // Which needs to be done before, and which after calling choke
    // manager?
    m_downUnchoked = false;

    download_queue()->cancel();
    m_download->download_choke_manager()->set_not_queued(this, &m_downChoke);
    m_download->download_throttle()->erase(m_peerChunks.download_throttle());

    return true;

  case ProtocolBase::UNCHOKE:
    if (type != Download::CONNECTION_LEECH)
      return true;

    m_downUnchoked = true;

    // Some peers unchoke us even though we're not interested, so we
    // need to ensure it doesn't get added to the queue.
    if (!m_downInterested)
      return true;

    m_download->download_choke_manager()->set_queued(this, &m_downChoke);
    return true;

  case ProtocolBase::INTERESTED:
    if (type == Download::CONNECTION_LEECH && m_peerChunks.bitfield()->is_all_set())
      return true;

    m_download->upload_choke_manager()->set_queued(this, &m_upChoke);
    return true;

  case ProtocolBase::NOT_INTERESTED:
    m_download->upload_choke_manager()->set_not_queued(this, &m_upChoke);
    return true;

  case ProtocolBase::HAVE:
    if (!m_down->can_read_have_body())
      break;

    read_have_chunk(buf->read_32());
    return true;

  case ProtocolBase::REQUEST:
    if (!m_down->can_read_request_body())
      break;

    if (!m_upChoke.choked()) {
      write_insert_poll_safe();
      read_request_piece(m_down->read_request());

    } else {
      m_down->read_request();
    }

    return true;

  case ProtocolBase::PIECE:
    if (type != Download::CONNECTION_LEECH)
      throw communication_error("Received a piece but the connection is strictly for seeding.");

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
        m_download->download_throttle()->insert(m_peerChunks.download_throttle());
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

  case ProtocolBase::PORT:
    if (!m_down->can_read_port_body())
      break;

    manager->dht_manager()->add_node(m_peerInfo->socket_address(), m_down->buffer()->read_16());
    return true;

  case ProtocolBase::EXTENSION_PROTOCOL:
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

    if (down_extension())
      m_down->set_state(ProtocolRead::IDLE);

    return true;

  default:
    throw communication_error("Received unsupported message type.");
  }

  // We were unsuccessfull in reading the message, need more data.
  buf->set_position_itr(beginning);
  return false;
}

template<Download::ConnectionType type>
void
PeerConnection<type>::event_read() {
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
          m_download->download_throttle()->node_used_unthrottled(length);

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
        if (type != Download::CONNECTION_LEECH)
          return;

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
        if (type != Download::CONNECTION_LEECH)
          return;

        if (download_queue()->transfer()->is_leader()) {
          m_down->set_state(ProtocolRead::READ_PIECE);
          break;
        }

        if (!down_chunk_skip())
          return;

        m_tryRequest = true;
        m_down->set_state(ProtocolRead::IDLE);
        down_chunk_finished();
        break;

      case ProtocolRead::READ_EXTENSION:
        if (!down_extension())
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

template<Download::ConnectionType type>
inline void
PeerConnection<type>::fill_write_buffer() {
  ProtocolBuffer<512>::iterator old_end = m_up->buffer()->end();

  // No need to use delayed choke ever.
  if (m_sendChoked && m_up->can_write_choke()) {
    m_sendChoked = false;
    m_up->write_choke(m_upChoke.choked());

    if (m_upChoke.choked()) {
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
  if (type == Download::CONNECTION_LEECH && m_sendInterested && m_up->can_write_interested()) {
    m_up->write_interested(m_downInterested);
    m_sendInterested = false;
  }

  if (type == Download::CONNECTION_LEECH && m_tryRequest) {
    if (!(m_tryRequest = !should_request()) &&
        !(m_tryRequest = try_request_pieces()) &&

        !download_queue()->is_interested_in_active()) {
      m_sendInterested = true;
      m_downInterested = false;

      m_download->download_choke_manager()->set_not_queued(this, &m_downChoke);
    }
  }

  DownloadMain::have_queue_type* haveQueue = m_download->have_queue();

  if (type == Download::CONNECTION_LEECH && 
      !haveQueue->empty() &&
      m_peerChunks.have_timer() <= haveQueue->front().first &&
      m_up->can_write_have()) {
    DownloadMain::have_queue_type::iterator last = std::find_if(haveQueue->begin(), haveQueue->end(),
                                                                rak::greater(m_peerChunks.have_timer(), rak::mem_ref(&DownloadMain::have_queue_type::value_type::first)));

    do {
      m_up->write_have((--last)->second);
    } while (last != haveQueue->begin() && m_up->can_write_have());

    m_peerChunks.set_have_timer(last->first + 1);
  }

  if (type == Download::CONNECTION_INITIAL_SEED && m_up->can_write_have())
    offer_chunk();

  while (type == Download::CONNECTION_LEECH && !m_peerChunks.cancel_queue()->empty() && m_up->can_write_cancel()) {
    m_up->write_cancel(m_peerChunks.cancel_queue()->front());
    m_peerChunks.cancel_queue()->pop_front();
  }

  if (m_sendPEXMask && m_up->can_write_extension() &&
      send_pex_message()) {
    // Don't do anything else if send_pex_message() succeeded.

  } else if (!m_upChoke.choked() &&
             !m_peerChunks.upload_queue()->empty() &&
             m_up->can_write_piece() &&
             (type != Download::CONNECTION_INITIAL_SEED || should_upload())) {
    write_prepare_piece();
  }

  if (is_encrypted())
    m_encryption.encrypt(old_end, m_up->buffer()->end() - old_end);
}

template<Download::ConnectionType type>
void
PeerConnection<type>::event_write() {
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
        if (!m_up->buffer()->consume(m_download->upload_throttle()->node_used_unthrottled(write_stream_throws(m_up->buffer()->position(), m_up->buffer()->remaining()))))
          return;

        m_up->buffer()->reset();

        if (m_up->last_command() == ProtocolBase::PIECE) {
          // We're uploading a piece.
          load_up_chunk();
          m_up->set_state(ProtocolWrite::WRITE_PIECE);

          // fall through to WRITE_PIECE case below

        } else if (m_up->last_command() == ProtocolBase::EXTENSION_PROTOCOL) {
          m_up->set_state(ProtocolWrite::WRITE_EXTENSION);
          break;

        } else {
          // Break or loop? Might do an ifelse based on size of the
          // write buffer. Also the write buffer is relatively large.
          m_up->set_state(ProtocolWrite::IDLE);
          break;
        }

      case ProtocolWrite::WRITE_PIECE:
        if (!up_chunk())
          return;

        m_up->set_state(ProtocolWrite::IDLE);
        break;

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

template<Download::ConnectionType type>
void
PeerConnection<type>::read_have_chunk(uint32_t index) {
  if (index >= m_peerChunks.bitfield()->size_bits())
    throw communication_error("Peer sent HAVE message with out-of-range index.");

  if (m_peerChunks.bitfield()->get(index))
    return;

  m_download->chunk_statistics()->received_have_chunk(&m_peerChunks, index, m_download->file_list()->chunk_size());

  if (type == Download::CONNECTION_INITIAL_SEED)
    m_download->initial_seeding()->chunk_seen(index, this);

  // Disconnect seeds when we are seeding (but not for initial seeding
  // so that we keep accurate chunk statistics until that is done).
  if (m_peerChunks.bitfield()->is_all_set()) {
    if (type == Download::CONNECTION_SEED || 
        (type != Download::CONNECTION_INITIAL_SEED && m_download->file_list()->is_done()))
      throw close_connection();

    m_download->upload_choke_manager()->set_not_queued(this, &m_upChoke);
  }

  if (type != Download::CONNECTION_LEECH || m_download->file_list()->is_done())
    return;

  if (is_down_interested()) {

    if (!m_tryRequest && m_download->chunk_selector()->received_have_chunk(&m_peerChunks, index)) {
      m_tryRequest = true;
      write_insert_poll_safe();
    }

  } else {

    if (m_download->chunk_selector()->received_have_chunk(&m_peerChunks, index)) {
      m_sendInterested = !m_downInterested;
      m_downInterested = true;
      
      // Ensure we get inserted into the choke manager queue in case
      // the peer keeps us unchoked even though we've said we're not
      // interested.
      if (m_downUnchoked)
        m_download->download_choke_manager()->set_queued(this, &m_downChoke);

      // Is it enough to insert into write here? Make the interested
      // check branch to include insert_write, even when not sending
      // interested.
      m_tryRequest = true;
      write_insert_poll_safe();
    }
  }
}

template<>
void
PeerConnection<Download::CONNECTION_INITIAL_SEED>::offer_chunk() {
  // If bytes left to send in this chunk minus bytes about to be sent is zero,
  // assume the peer will have got the chunk completely. In that case we may
  // get another one to offer if not enough other peers are interested even
  // if the peer would otherwise still be blocked.
  uint32_t bytesLeft = m_data.bytesLeft;
  if (!m_peerChunks.upload_queue()->empty() && m_peerChunks.upload_queue()->front().index() == m_data.lastIndex)
    bytesLeft -= m_peerChunks.upload_queue()->front().length();

  uint32_t index = m_download->initial_seeding()->chunk_offer(this, bytesLeft == 0 ? m_data.lastIndex : InitialSeeding::no_offer);

  if (index == InitialSeeding::no_offer || index == m_data.lastIndex)
    return;

  m_up->write_have(index);
  m_data.lastIndex = index;
  m_data.bytesLeft = m_download->file_list()->chunk_index_size(index);
}

template<>
bool
PeerConnection<Download::CONNECTION_INITIAL_SEED>::should_upload() {
  // For initial seeding, check if chunk is well seeded now, and if so
  // remove it from the queue to better use our bandwidth on rare chunks.
  while (!m_peerChunks.upload_queue()->empty() &&
         !m_download->initial_seeding()->should_upload(m_peerChunks.upload_queue()->front().index()))
    m_peerChunks.upload_queue()->pop_front();

  // If queue ends up empty, choke peer to let it know that it
  // shouldn't wait for the cancelled pieces to be sent.
  if (m_peerChunks.upload_queue()->empty()) {
    m_download->upload_choke_manager()->set_not_queued(this, &m_upChoke);
    m_download->upload_choke_manager()->set_queued(this, &m_upChoke);

  // If we're sending the chunk we last offered, adjust bytes left in it.
  } else if (m_peerChunks.upload_queue()->front().index() == m_data.lastIndex) {
    m_data.bytesLeft -= m_peerChunks.upload_queue()->front().length();

    if (!m_data.bytesLeft)
      m_data.lastIndex = InitialSeeding::no_offer;
  }

  return !m_peerChunks.upload_queue()->empty();
}

// Explicit instantiation of the member functions and vtable.
template class PeerConnection<Download::CONNECTION_LEECH>;
template class PeerConnection<Download::CONNECTION_SEED>;
template class PeerConnection<Download::CONNECTION_INITIAL_SEED>;

}
