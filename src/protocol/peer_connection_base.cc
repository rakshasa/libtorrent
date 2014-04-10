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

#define __STDC_FORMAT_MACROS

#include <cstdio>
#include <fcntl.h>
#include <rak/error_number.h>
#include <rak/functional.h>
#include <rak/string_manip.h>

#include "data/chunk_iterator.h"
#include "data/chunk_list.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_main.h"
#include "net/socket_base.h"
#include "torrent/exceptions.h"
#include "torrent/data/block.h"
#include "torrent/chunk_manager.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/throttle.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/connection_list.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

#include "extensions.h"
#include "peer_connection_base.h"

#include "manager.h"

#define LT_LOG_PIECE_EVENTS(log_fmt, ...)                               \
  lt_log_print_info(LOG_PROTOCOL_PIECE_EVENTS, this->download()->info(), "piece_events", "%40s " log_fmt, this->peer_info()->id_hex(), __VA_ARGS__);

namespace torrent {

inline void
log_mincore_stats_func(bool is_incore, bool new_index, bool& continous) {
  if (!new_index && is_incore) {
    instrumentation_update(INSTRUMENTATION_MINCORE_INCORE_TOUCHED, 1);
  }
  if (new_index && is_incore) {
    instrumentation_update(INSTRUMENTATION_MINCORE_INCORE_NEW, 1);
  }
  if (!new_index && !is_incore) {
    instrumentation_update(INSTRUMENTATION_MINCORE_NOT_INCORE_TOUCHED, 1);
  }
  if (new_index && !is_incore) {
    instrumentation_update(INSTRUMENTATION_MINCORE_NOT_INCORE_NEW, 1);
  }
  if (continous && !is_incore) {
    instrumentation_update(INSTRUMENTATION_MINCORE_INCORE_BREAK, 1);
  }

  continous = is_incore;
}

PeerConnectionBase::PeerConnectionBase() :
  m_download(NULL),
  
  m_down(new ProtocolRead()),
  m_up(new ProtocolWrite()),

  m_downStall(0),

  m_downInterested(false),
  m_downUnchoked(false),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true),
  m_sendPEXMask(0),

  m_encryptBuffer(NULL),
  m_extensions(NULL),

  m_incoreContinous(false) {

  m_peerInfo = NULL;
}

PeerConnectionBase::~PeerConnectionBase() {
  delete m_up;
  delete m_down;

  delete m_encryptBuffer;

  if (m_extensions != NULL && !m_extensions->is_default())
    delete m_extensions;

  m_extensionMessage.clear();
}

void
PeerConnectionBase::initialize(DownloadMain* download, PeerInfo* peerInfo, SocketFd fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions) {
  if (get_fd().is_valid())
    throw internal_error("Tried to re-set PeerConnection.");

  if (!fd.is_valid())
    throw internal_error("PeerConnectionBase::set(...) received bad input.");

  if (encryptionInfo->is_encrypted() != encryptionInfo->decrypt_valid())
    throw internal_error("Encryption and decryption inconsistent.");

  set_fd(fd);

  m_peerInfo = peerInfo;
  m_download = download;

  m_encryption = *encryptionInfo;
  m_extensions = extensions;

  m_extensions->set_connection(this);

  m_upChoke.set_entry(m_download->up_group_entry());
  m_downChoke.set_entry(m_download->down_group_entry());

  m_peerChunks.set_peer_info(m_peerInfo);
  m_peerChunks.bitfield()->swap(*bitfield);

  std::pair<ThrottleList*, ThrottleList*> throttles = m_download->throttles(m_peerInfo->socket_address());
  m_up->set_throttle(throttles.first);
  m_down->set_throttle(throttles.second);

  m_peerChunks.upload_throttle()->set_list_iterator(m_up->throttle()->end());
  m_peerChunks.upload_throttle()->slot_activate() = std::tr1::bind(&SocketBase::receive_throttle_up_activate, static_cast<SocketBase*>(this));

  m_peerChunks.download_throttle()->set_list_iterator(m_down->throttle()->end());
  m_peerChunks.download_throttle()->slot_activate() = std::tr1::bind(&SocketBase::receive_throttle_down_activate, static_cast<SocketBase*>(this));

  request_list()->set_delegator(m_download->delegator());
  request_list()->set_peer_chunks(&m_peerChunks);

  try {
    initialize_custom();

  } catch (close_connection& e) {
    // The handshake manager closes the socket for us.
    m_peerInfo = NULL;
    m_download = NULL;
    m_extensions = NULL;

    get_fd().clear();
    return;
  }

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_write(this);
  manager->poll()->insert_error(this);

  m_timeLastRead = cachedTime;

  m_download->chunk_statistics()->received_connect(&m_peerChunks);

  // Hmm... cleanup?
//   update_interested();

  m_peerChunks.download_cache()->clear();

  if (!m_download->file_list()->is_done()) {
    m_sendInterested = true;
    m_downInterested = true;
  }
}

void
PeerConnectionBase::cleanup() {
  if (!get_fd().is_valid())
    return;

  if (m_download == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  // TODO: Verify that transfer counter gets modified by this...
  m_request_list.clear();

  up_chunk_release();
  down_chunk_release();

  m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() - m_upChoke.unchoked());
  m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() - m_downChoke.unchoked());

  m_download->choke_group()->up_queue()->disconnected(this, &m_upChoke);
  m_download->choke_group()->down_queue()->disconnected(this, &m_downChoke);
  m_download->chunk_statistics()->received_disconnect(&m_peerChunks);

  if (!m_extensions->is_default())
    m_extensions->cleanup();

  manager->poll()->remove_read(this);
  manager->poll()->remove_write(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);
  
  manager->connection_manager()->dec_socket_count();

  get_fd().close();
  get_fd().clear();

  m_up->throttle()->erase(m_peerChunks.upload_throttle());
  m_down->throttle()->erase(m_peerChunks.download_throttle());

  m_up->set_state(ProtocolWrite::INTERNAL_ERROR);
  m_down->set_state(ProtocolRead::INTERNAL_ERROR);

  m_download = NULL;
}

void
PeerConnectionBase::set_upload_snubbed(bool v) {
  if (v)
    m_download->choke_group()->up_queue()->set_snubbed(this, &m_upChoke);
  else
    m_download->choke_group()->up_queue()->set_not_snubbed(this, &m_upChoke);
}

bool
PeerConnectionBase::receive_upload_choke(bool choke) {
  if (choke == m_upChoke.choked())
    throw internal_error("PeerConnectionBase::receive_upload_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_sendChoked = true;
  m_upChoke.set_unchoked(!choke);
  m_upChoke.set_time_last_choke(cachedTime.usec());

  if (choke) {
    m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() - 1);
    m_upChoke.entry()->connection_choked(this);
    m_upChoke.entry()->connection_queued(this);

    m_download->choke_group()->up_queue()->modify_currently_unchoked(-1);
    m_download->choke_group()->up_queue()->modify_currently_queued(1);

  } else {
    m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() + 1);
    m_upChoke.entry()->connection_unqueued(this);
    m_upChoke.entry()->connection_unchoked(this);

    m_download->choke_group()->up_queue()->modify_currently_unchoked(1);
    m_download->choke_group()->up_queue()->modify_currently_queued(-1);
  }

  return true;
}

bool
PeerConnectionBase::receive_download_choke(bool choke) {
  if (choke == m_downChoke.choked())
    throw internal_error("PeerConnectionBase::receive_download_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_downChoke.set_unchoked(!choke);
  m_downChoke.set_time_last_choke(cachedTime.usec());

  if (choke) {
    m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() - 1);
    m_downChoke.entry()->connection_choked(this);
    m_downChoke.entry()->connection_queued(this);

    m_download->choke_group()->down_queue()->modify_currently_unchoked(-1);
    m_download->choke_group()->down_queue()->modify_currently_queued(1);

  } else {
    m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() + 1);
    m_downChoke.entry()->connection_unqueued(this);
    m_downChoke.entry()->connection_unchoked(this);

    m_download->choke_group()->down_queue()->modify_currently_unchoked(1);
    m_download->choke_group()->down_queue()->modify_currently_queued(-1);
  }

  if (choke) {
    m_peerChunks.download_cache()->disable();

    // If the queue isn't empty, then we might still receive some
    // pieces, so don't remove us from throttle or release the chunk.
    if (!request_list()->is_downloading() && request_list()->queued_empty()) {
      m_down->throttle()->erase(m_peerChunks.download_throttle());
      down_chunk_release();
    }

    // Send uninterested if unchoked, but only _after_ receiving our
    // chunks?

    if (m_downUnchoked) {
      // Tell the peer we're no longer interested to avoid
      // disconnects. We keep the connection in the queue so that
      // ChokeManager::cycle(...) can attempt to get us unchoked
      // again.

      m_sendInterested = m_downInterested;
      m_downInterested = false;

    } else {
      // Remove from queue so that an unchoke from the remote peer
      // will cause the connection to be unchoked immediately by the
      // choke manager.
      //
      // TODO: This doesn't seem safe...
      m_download->choke_group()->down_queue()->set_not_queued(this, &m_downChoke);
      return false;
    }

  } else {
    m_tryRequest = true;

    if (!m_downInterested) {
      // We were marked as not interested by the cycling choke and
      // kept in the queue, thus the peer should have some pieces of
      // interest.
      //
      // We have now been 'unchoked' by the choke manager, so tell the
      // peer that we're again interested. If the peer doesn't unchoke
      // us within a cycle or two we're likely to be choked and left
      // out of the queue. So if the peer unchokes us at a later time,
      // we skip the queue and unchoke immediately.

      m_sendInterested = !m_downInterested;
      m_downInterested = true;
    }
  }

  return true;
}

void
PeerConnectionBase::load_up_chunk() {
  if (m_upChunk.is_valid() && m_upChunk.index() == m_upPiece.index()) {
    // Better checking needed.
    //     m_upChunk.chunk()->preload(m_upPiece.offset(), m_upChunk.chunk()->size());

    if (lt_log_is_valid(LOG_INSTRUMENTATION_MINCORE))
      log_mincore_stats_func(m_upChunk.chunk()->is_incore(m_upPiece.offset(), m_upPiece.length()), false, m_incoreContinous);

    return;
  }

  up_chunk_release();
  
  m_upChunk = m_download->chunk_list()->get(m_upPiece.index());
  
  if (!m_upChunk.is_valid())
    throw storage_error("File chunk read error: " + std::string(m_upChunk.error_number().c_str()));

  if (is_encrypted() && m_encryptBuffer == NULL) {
    m_encryptBuffer = new EncryptBuffer();
    m_encryptBuffer->reset();
  }

  m_incoreContinous = false;

  if (lt_log_is_valid(LOG_INSTRUMENTATION_MINCORE))
    log_mincore_stats_func(m_upChunk.chunk()->is_incore(m_upPiece.offset(), m_upPiece.length()), true, m_incoreContinous);

  m_incoreContinous = true;

  // Also check if we've already preloaded in the recent past, even
  // past unmaps.
  ChunkManager* cm = manager->chunk_manager();
  uint32_t preloadSize = m_upChunk.chunk()->chunk_size() - m_upPiece.offset();

  if (cm->preload_type() == 0 ||
      m_upChunk.object()->time_preloaded() >= cachedTime - rak::timer::from_seconds(60) ||

      preloadSize < cm->preload_min_size() ||
      m_peerChunks.upload_throttle()->rate()->rate() < cm->preload_required_rate() * ((preloadSize + (2 << 20) - 1) / (2 << 20))) {
    cm->inc_stats_not_preloaded();
    return;
  }

  cm->inc_stats_preloaded();

  m_upChunk.object()->set_time_preloaded(cachedTime);
  m_upChunk.chunk()->preload(m_upPiece.offset(), m_upChunk.chunk()->chunk_size(), cm->preload_type() == 1);
}

void
PeerConnectionBase::cancel_transfer(BlockTransfer* transfer) {
  if (!get_fd().is_valid())
    throw internal_error("PeerConnectionBase::cancel_transfer(...) !get_fd().is_valid()");

  if (transfer->peer_info() != peer_info())
    throw internal_error("PeerConnectionBase::cancel_transfer(...) peer info doesn't match");

  // We don't send cancel messages if the transfer has already
  // started.
  if (transfer == m_request_list.transfer())
    return;

  write_insert_poll_safe();

  m_peerChunks.cancel_queue()->push_back(transfer->piece());
}

void
PeerConnectionBase::event_error() {
  m_download->connection_list()->erase(this, 0);
}

bool
PeerConnectionBase::should_connection_unchoke(choke_queue* cq) const {
  if (cq == m_download->choke_group()->up_queue())
    return m_download->info()->upload_unchoked() < m_download->up_group_entry()->max_slots();

  if (cq == m_download->choke_group()->down_queue())
    return m_download->info()->download_unchoked() < m_download->down_group_entry()->max_slots();

  return true;
}

bool
PeerConnectionBase::down_chunk_start(const Piece& piece) {
  if (!request_list()->downloading(piece)) {
    if (piece.length() == 0) {
      LT_LOG_PIECE_EVENTS("(down) skipping_empty %" PRIu32 " %" PRIu32 " %" PRIu32,
                          piece.index(), piece.offset(), piece.length());
    } else {
      LT_LOG_PIECE_EVENTS("(down) skipping_unneeded %" PRIu32 " %" PRIu32 " %" PRIu32,
                          piece.index(), piece.offset(), piece.length());
    }

    return false;
  }

  if (!m_download->file_list()->is_valid_piece(piece))
    throw internal_error("Incoming pieces list contains a bad piece.");
  
  if (!m_downChunk.is_valid() || piece.index() != m_downChunk.index()) {
    down_chunk_release();
    m_downChunk = m_download->chunk_list()->get(piece.index(), ChunkList::get_writable);
  
    if (!m_downChunk.is_valid())
      throw storage_error("File chunk write error: " + std::string(m_downChunk.error_number().c_str()) + ".");
  }

  LT_LOG_PIECE_EVENTS("(down) %s %" PRIu32 " %" PRIu32 " %" PRIu32,
                      request_list()->transfer()->is_leader() ? "started_on" : "skipping_partial",
                      piece.index(), piece.offset(), piece.length());

  return request_list()->transfer()->is_leader();
}

void
PeerConnectionBase::down_chunk_finished() {
  if (!request_list()->transfer()->is_finished())
    throw internal_error("PeerConnectionBase::down_chunk_finished() Transfer not finished.");

  BlockTransfer* transfer = request_list()->transfer();
  
  LT_LOG_PIECE_EVENTS("(down) %s %" PRIu32 " %" PRIu32 " %" PRIu32,
                      transfer->is_leader() ? "completed " : "skipped  ",
                      transfer->piece().index(), transfer->piece().offset(), transfer->piece().length());

  if (transfer->is_leader()) {
    if (!m_downChunk.is_valid())
      throw internal_error("PeerConnectionBase::down_chunk_finished() Transfer is the leader, but no chunk allocated.");

    request_list()->finished();
    m_downChunk.object()->set_time_modified(cachedTime);

  } else {
    request_list()->skipped();
  }
        
  if (m_downStall > 0)
    m_downStall--;
        
  // We need to release chunks when we're not sure if they will be
  // used in the near future so as to avoid hitting the address space
  // limit in high-bandwidth situations.
  //
  // Some tweaking of the pipe size might be necessary if the queue
  // empties too often.
  if (m_downChunk.is_valid() &&
      (request_list()->queued_empty() || m_downChunk.index() != request_list()->next_queued_piece().index()))
    down_chunk_release();

  // If we were choked by choke_manager but still had queued pieces,
  // then we might still be in the throttle.
  if (m_downChoke.choked() && request_list()->queued_empty())
    m_down->throttle()->erase(m_peerChunks.download_throttle());

  write_insert_poll_safe();
}

bool
PeerConnectionBase::down_chunk() {
  if (!m_down->throttle()->is_throttled(m_peerChunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk() tried to read a piece but is not in throttle list");

  if (!m_downChunk.chunk()->is_writable())
    throw internal_error("PeerConnectionBase::down_part() chunk not writable, permission denided");

  uint32_t quota = m_down->throttle()->node_quota(m_peerChunks.download_throttle());

  if (quota == 0) {
    manager->poll()->remove_read(this);
    m_down->throttle()->node_deactivate(m_peerChunks.download_throttle());
    return false;
  }

  uint32_t bytesTransfered = 0;
  BlockTransfer* transfer = m_request_list.transfer();

  Chunk::data_type data;
  ChunkIterator itr(m_downChunk.chunk(),
                    transfer->piece().offset() + transfer->position(),
                    transfer->piece().offset() + std::min(transfer->position() + quota, transfer->piece().length()));

  do {
    data = itr.data();
    data.second = read_stream_throws(data.first, data.second);

    if (is_encrypted())
      m_encryption.decrypt(data.first, data.second);

    bytesTransfered += data.second;

  } while (data.second != 0 && itr.forward(data.second));

  transfer->adjust_position(bytesTransfered);

  m_down->throttle()->node_used(m_peerChunks.download_throttle(), bytesTransfered);
  m_download->info()->mutable_down_rate()->insert(bytesTransfered);

  return transfer->is_finished();
}

bool
PeerConnectionBase::down_chunk_from_buffer() {
  m_down->buffer()->consume(down_chunk_process(m_down->buffer()->position(), m_down->buffer()->remaining()));

  if (!m_request_list.transfer()->is_finished() && m_down->buffer()->remaining() != 0)
    throw internal_error("PeerConnectionBase::down_chunk_from_buffer() !transfer->is_finished() && m_down->buffer()->remaining() != 0.");

  return m_request_list.transfer()->is_finished();
}  

// When this transfer again becomes the leader, we just return false
// and wait for the next polling. It is an exceptional case so we
// don't really care that much about performance.
bool
PeerConnectionBase::down_chunk_skip() {
  ThrottleList* throttle = m_down->throttle();

  if (!throttle->is_throttled(m_peerChunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk_skip() tried to read a piece but is not in throttle list");

  uint32_t quota = throttle->node_quota(m_peerChunks.download_throttle());

  if (quota == 0) {
    manager->poll()->remove_read(this);
    throttle->node_deactivate(m_peerChunks.download_throttle());
    return false;
  }

  uint32_t length = read_stream_throws(m_nullBuffer, std::min(quota, m_request_list.transfer()->piece().length() - m_request_list.transfer()->position()));
  throttle->node_used(m_peerChunks.download_throttle(), length);

  if (is_encrypted())
    m_encryption.decrypt(m_nullBuffer, length);

  if (down_chunk_skip_process(m_nullBuffer, length) != length)
    throw internal_error("PeerConnectionBase::down_chunk_skip() down_chunk_skip_process(m_nullBuffer, length) != length.");

  return m_request_list.transfer()->is_finished();
}

bool
PeerConnectionBase::down_chunk_skip_from_buffer() {
  m_down->buffer()->consume(down_chunk_skip_process(m_down->buffer()->position(), m_down->buffer()->remaining()));
  
  return m_request_list.transfer()->is_finished();
}

// Process data from a leading transfer.
uint32_t
PeerConnectionBase::down_chunk_process(const void* buffer, uint32_t length) {
  if (!m_downChunk.is_valid() || m_downChunk.index() != m_request_list.transfer()->index())
    throw internal_error("PeerConnectionBase::down_chunk_process(...) !m_downChunk.is_valid() || m_downChunk.index() != m_request_list.transfer()->index().");

  if (length == 0)
    return length;

  BlockTransfer* transfer = m_request_list.transfer();

  length = std::min(transfer->piece().length() - transfer->position(), length);

  m_downChunk.chunk()->from_buffer(buffer, transfer->piece().offset() + transfer->position(), length);

  transfer->adjust_position(length);

  m_down->throttle()->node_used(m_peerChunks.download_throttle(), length);
  m_download->info()->mutable_down_rate()->insert(length);

  return length;
}

// Process data from non-leading transfer. If this transfer encounters
// mismatching data with the leader then bork this transfer. If we get
// ahead of the leader, we switch the leader.
uint32_t
PeerConnectionBase::down_chunk_skip_process(const void* buffer, uint32_t length) {
  BlockTransfer* transfer = m_request_list.transfer();

  // Adjust 'length' to be less than or equal to what is remaining of
  // the block to simplify the rest of the function.
  length = std::min(length, transfer->piece().length() - transfer->position());

  // Hmm, this might result in more bytes than nessesary being
  // counted.
  m_down->throttle()->node_used(m_peerChunks.download_throttle(), length);
  m_download->info()->mutable_down_rate()->insert(length);
  m_download->info()->mutable_skip_rate()->insert(length);

  if (!transfer->is_valid()) {
    transfer->adjust_position(length);
    return length;
  }

  if (!transfer->block()->is_transfering())
    throw internal_error("PeerConnectionBase::down_chunk_skip_process(...) block is not transferring, yet we have non-leaders.");

  // Temporary test.
  if (transfer->position() > transfer->block()->leader()->position())
    throw internal_error("PeerConnectionBase::down_chunk_skip_process(...) transfer is past the Block's position.");

  // If the transfer is valid, compare the downloaded data to the
  // leader.
  uint32_t compareLength = std::min(length, transfer->block()->leader()->position() - transfer->position());

  // The data doesn't match with what has previously been downloaded,
  // bork this transfer.
  if (!m_downChunk.chunk()->compare_buffer(buffer, transfer->piece().offset() + transfer->position(), compareLength)) {
    LT_LOG_PIECE_EVENTS("(down) download_data_mismatch %" PRIu32 " %" PRIu32 " %" PRIu32,
                        transfer->piece().index(), transfer->piece().offset(), transfer->piece().length());
    
    m_request_list.transfer_dissimilar();
    m_request_list.transfer()->adjust_position(length);

    return length;
  }

  transfer->adjust_position(compareLength);

  if (compareLength == length)
    return length;

  // Add another check here to see if we really want to be the new
  // leader.

  transfer->block()->change_leader(transfer);

  if (down_chunk_process(static_cast<const char*>(buffer) + compareLength, length - compareLength) != length - compareLength)
    throw internal_error("PeerConnectionBase::down_chunk_skip_process(...) down_chunk_process(...) returned wrong value.");
  
  return length;
}

bool
PeerConnectionBase::down_extension() {
  if (m_down->buffer()->remaining()) {
    uint32_t need = std::min(m_extensions->read_need(), (uint32_t)m_down->buffer()->remaining());
    std::memcpy(m_extensions->read_position(), m_down->buffer()->position(), need);

    m_extensions->read_move(need);
    m_down->buffer()->consume(need);
  }

  if (!m_extensions->is_complete()) {
    uint32_t bytes = read_stream_throws(m_extensions->read_position(), m_extensions->read_need());
    m_down->throttle()->node_used_unthrottled(bytes);
    
    if (is_encrypted())
      m_encryption.decrypt(m_extensions->read_position(), bytes);

    m_extensions->read_move(bytes);
  }

  // If extension can't be processed yet (due to a pending write),
  // disable reads until the pending message is completely sent.
  if (m_extensions->is_complete() && !m_extensions->is_invalid() && !m_extensions->read_done()) {
    manager->poll()->remove_read(this);
    return false;
  }

  return m_extensions->is_complete();
}

inline uint32_t
PeerConnectionBase::up_chunk_encrypt(uint32_t quota) {
  if (m_encryptBuffer == NULL)
    throw internal_error("PeerConnectionBase::up_chunk: m_encryptBuffer is NULL.");

  if (quota <= m_encryptBuffer->remaining())
    return quota;

  // Also, consider checking here if the number of bytes remaining in
  // the buffer is small enought that the cost of moving them would
  // outweigh the extra context switches, etc.

  if (m_encryptBuffer->remaining() == 0) {
    // This handles reset also for new chunk transfers.
    m_encryptBuffer->reset();

    quota = std::min<uint32_t>(quota, m_encryptBuffer->reserved());

  } else {
    quota = std::min<uint32_t>(quota - m_encryptBuffer->remaining(), m_encryptBuffer->reserved_left());
  }

  m_upChunk.chunk()->to_buffer(m_encryptBuffer->end(), m_upPiece.offset() + m_encryptBuffer->remaining(), quota);
  m_encryption.encrypt(m_encryptBuffer->end(), quota);
  m_encryptBuffer->move_end(quota);

  return m_encryptBuffer->remaining();
}

bool
PeerConnectionBase::up_chunk() {
  if (!m_up->throttle()->is_throttled(m_peerChunks.upload_throttle()))
    throw internal_error("PeerConnectionBase::up_chunk() tried to write a piece but is not in throttle list");

  if (!m_upChunk.chunk()->is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  uint32_t quota = m_up->throttle()->node_quota(m_peerChunks.upload_throttle());

  if (quota == 0) {
    manager->poll()->remove_write(this);
    m_up->throttle()->node_deactivate(m_peerChunks.upload_throttle());
    return false;
  }

  uint32_t bytesTransfered = 0;

  if (is_encrypted()) {
    // Prepare as many bytes as quota specifies, up to end of piece or
    // buffer. Only bytes beyond remaining() are new and will be
    // encrypted.
    quota = up_chunk_encrypt(std::min(quota, m_upPiece.length()));

    bytesTransfered = write_stream_throws(m_encryptBuffer->position(), quota);
    m_encryptBuffer->consume(bytesTransfered);

  } else {
    Chunk::data_type data;
    ChunkIterator itr(m_upChunk.chunk(), m_upPiece.offset(), m_upPiece.offset() + std::min(quota, m_upPiece.length()));

    do {
      data = itr.data();
      data.second = write_stream_throws(data.first, data.second);

      bytesTransfered += data.second;

    } while (data.second != 0 && itr.forward(data.second));
  }

  m_up->throttle()->node_used(m_peerChunks.upload_throttle(), bytesTransfered);
  m_download->info()->mutable_up_rate()->insert(bytesTransfered);

  // Just modifying the piece to cover the remaining data ends up
  // being much cleaner and we avoid an unnessesary position variable.
  m_upPiece.set_offset(m_upPiece.offset() + bytesTransfered);
  m_upPiece.set_length(m_upPiece.length() - bytesTransfered);

  return m_upPiece.length() == 0;
}

bool
PeerConnectionBase::up_extension() {
  if (m_extensionOffset == extension_must_encrypt) {
    if (m_extensionMessage.owned()) {
      m_encryption.encrypt(m_extensionMessage.data(), m_extensionMessage.length());

    } else {
      char* buffer = new char[m_extensionMessage.length()];

      m_encryption.encrypt(m_extensionMessage.data(), buffer, m_extensionMessage.length());
      m_extensionMessage.set(buffer, buffer + m_extensionMessage.length(), true);
    }

    m_extensionOffset = 0;
  }

  if (m_extensionOffset >= m_extensionMessage.length())
    throw internal_error("PeerConnectionBase::up_extension bad offset.");

  uint32_t written = write_stream_throws(m_extensionMessage.data() + m_extensionOffset, m_extensionMessage.length() - m_extensionOffset);
  m_up->throttle()->node_used_unthrottled(written);
  m_extensionOffset += written;

  if (m_extensionOffset < m_extensionMessage.length())
    return false;

  m_extensionMessage.clear();

  // If we have an unprocessed message, process it now and enable reads again.
  if (m_extensions->is_complete() && !m_extensions->is_invalid()) {
    // DEBUG: What, this should fail when we block, no?
    if (!m_extensions->read_done())
      throw internal_error("PeerConnectionBase::up_extension could not process complete extension message.");

    manager->poll()->insert_read(this);
  }

  return true;
}

void
PeerConnectionBase::down_chunk_release() {
  if (m_downChunk.is_valid())
    m_download->chunk_list()->release(&m_downChunk);
}

void
PeerConnectionBase::up_chunk_release() {
  if (m_upChunk.is_valid())
    m_download->chunk_list()->release(&m_upChunk);
}

void
PeerConnectionBase::read_request_piece(const Piece& p) {
  PeerChunks::piece_list_type::iterator itr = std::find(m_peerChunks.upload_queue()->begin(),
                                                        m_peerChunks.upload_queue()->end(),
                                                        p);
  
  if (m_upChoke.choked() || itr != m_peerChunks.upload_queue()->end() || p.length() > (1 << 17)) {
    LT_LOG_PIECE_EVENTS("(up)   request_ignored  %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
    return;
  }

  m_peerChunks.upload_queue()->push_back(p);
  write_insert_poll_safe();

  LT_LOG_PIECE_EVENTS("(up)   request_added    %" PRIu32 " %" PRIu32 " %" PRIu32,
                      p.index(), p.offset(), p.length());
}

void
PeerConnectionBase::read_cancel_piece(const Piece& p) {
  PeerChunks::piece_list_type::iterator itr = std::find(m_peerChunks.upload_queue()->begin(),
                                                        m_peerChunks.upload_queue()->end(),
                                                        p);
  
  if (itr != m_peerChunks.upload_queue()->end()) {
    m_peerChunks.upload_queue()->erase(itr);

    LT_LOG_PIECE_EVENTS("(up)   cancel_requested %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
  } else {
    LT_LOG_PIECE_EVENTS("(up)   cancel_ignored   %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
  }
}  

void
PeerConnectionBase::write_prepare_piece() {
  m_upPiece = m_peerChunks.upload_queue()->front();
  m_peerChunks.upload_queue()->pop_front();

  // Move these checks somewhere else?
  if (!m_download->file_list()->is_valid_piece(m_upPiece) ||
      !m_download->file_list()->bitfield()->get(m_upPiece.index())) {
    char buffer[128];

    snprintf(buffer, 128, "Peer requested an invalid piece: %u %u %u",
             m_upPiece.index(), m_upPiece.length(), m_upPiece.offset());

    LT_LOG_PIECE_EVENTS("(up)   invalid_piece_in_upload_queue %" PRIu32 " %" PRIu32 " %" PRIu32,
                        m_upPiece.index(), m_upPiece.length(), m_upPiece.offset());

    throw communication_error(buffer);
  }
  
  m_up->write_piece(m_upPiece);

  LT_LOG_PIECE_EVENTS("(up)   prepared         %" PRIu32 " %" PRIu32 " %" PRIu32,
                      m_upPiece.index(), m_upPiece.length(), m_upPiece.offset());
}

void
PeerConnectionBase::write_prepare_extension(int type, const DataBuffer& message) {
  m_up->write_extension(m_extensions->id(type), message.length());

  m_extensionOffset = 0;
  m_extensionMessage = message;

  // Need to encrypt the buffer, but not until the m_up
  // write buffer has been flushed, so flag it for now.
  if (is_encrypted())
    m_extensionOffset = extension_must_encrypt;
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
PeerConnectionBase::should_request() {
  if (m_downChoke.choked() || !m_downInterested || !m_downUnchoked)
    // || m_down->get_state() == ProtocolRead::READ_SKIP_PIECE)
    return false;

  else if (!m_download->delegator()->get_aggressive())
    return true;

  else
    // We check if the peer is stalled, if it is not then we should
    // request. If the peer is stalled then we only request if the
    // download rate is below a certain value.
    return m_downStall <= 1 || m_download->info()->down_rate()->rate() < (10 << 10);
}

bool
PeerConnectionBase::try_request_pieces() {
  if (request_list()->queued_empty())
    m_downStall = 0;

  uint32_t pipeSize = request_list()->calculate_pipe_size(m_peerChunks.download_throttle()->rate()->rate());

  // Don't start requesting if we can't do it in large enough chunks.
  if (request_list()->pipe_size() >= (pipeSize + 10) / 2)
    return false;

  bool success = false;

  while (request_list()->queued_size() < pipeSize && m_up->can_write_request()) {

    // Delegator should return a vector of pieces, and it should be
    // passed the number of pieces it should delegate. Try to ensure
    // it receives large enough request to fill a whole chunk if the
    // peer is fast enough.

    const Piece* p = request_list()->delegate();

    if (p == NULL)
      break;

    if (!m_download->file_list()->is_valid_piece(*p) || !m_peerChunks.bitfield()->get(p->index()))
      throw internal_error("PeerConnectionBase::try_request_pieces() tried to use an invalid piece.");

    m_up->write_request(*p);

    LT_LOG_PIECE_EVENTS("(down) requesting %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p->index(), p->offset(), p->length());
    success = true;
  }

  return success;
}

// Send one peer exchange message according to bits set in m_sendPEXMask.
// We can only send one message at a time, because the derived class
// needs to flush the buffer and call up_extension before the next one.
bool
PeerConnectionBase::send_pex_message() {
  if (!m_extensions->is_remote_supported(ProtocolExtension::UT_PEX)) {
    m_sendPEXMask = 0;
    return false;
  }

  // Message to tell peer to stop/start doing PEX is small so send it first.
  if (m_sendPEXMask & (PEX_ENABLE | PEX_DISABLE)) {
    if (!m_extensions->is_remote_supported(ProtocolExtension::UT_PEX))
      throw internal_error("PeerConnectionBase::send_pex_message() Not supported by peer.");

    write_prepare_extension(ProtocolExtension::HANDSHAKE,
                            ProtocolExtension::generate_toggle_message(ProtocolExtension::UT_PEX, (m_sendPEXMask & PEX_ENABLE) != 0));

    m_sendPEXMask &= ~(PEX_ENABLE | PEX_DISABLE);

  } else if (m_sendPEXMask & PEX_DO && m_extensions->id(ProtocolExtension::UT_PEX)) {
    const DataBuffer& pexMessage = m_download->get_ut_pex(m_extensions->is_initial_pex());
    m_extensions->clear_initial_pex();
 
    m_sendPEXMask &= ~PEX_DO;

    if (pexMessage.empty())
      return false;

    write_prepare_extension(ProtocolExtension::UT_PEX, pexMessage);

  } else {
    m_sendPEXMask = 0;
  }

  return true;
}

// Extension protocol needs to send a reply.
bool
PeerConnectionBase::send_ext_message() {
  write_prepare_extension(m_extensions->pending_message_type(), m_extensions->pending_message_data());
  m_extensions->clear_pending_message();
  return true;
}

void
PeerConnectionBase::receive_metadata_piece(uint32_t piece, const char* data, uint32_t length) {
}

}
