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

#include <cstdio>
#include <rak/error_number.h>

#include "torrent/exceptions.h"
#include "torrent/data/block.h"
#include "torrent/chunk_manager.h"
#include "data/chunk_iterator.h"
#include "data/chunk_list.h"
#include "download/choke_manager.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_info.h"
#include "download/download_main.h"
#include "net/socket_base.h"
#include "torrent/connection_manager.h"
#include "torrent/peer/peer_info.h"

#include "extensions.h"
#include "peer_connection_base.h"

#include "manager.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_download(NULL),
  
  m_down(new ProtocolRead()),
  m_up(new ProtocolWrite()),

  m_peerInfo(NULL),

  m_downStall(0),

  m_downInterested(false),
  m_downUnchoked(false),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true),
  m_sendPEXMask(0),

  m_encryptBuffer(NULL),
  m_extensions(NULL) {
}

PeerConnectionBase::~PeerConnectionBase() {
  delete m_up;
  delete m_down;

  delete m_encryptBuffer;

  if (m_extensions != NULL && !m_extensions->is_default())
    delete m_extensions;

  if (m_extensionMessage.copied())
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

  m_peerChunks.set_peer_info(m_peerInfo);
  m_peerChunks.bitfield()->swap(*bitfield);

  m_peerChunks.upload_throttle()->set_list_iterator(m_download->upload_throttle()->end());
  m_peerChunks.upload_throttle()->slot_activate(rak::make_mem_fun(this, &PeerConnectionBase::receive_throttle_up_activate));

  m_peerChunks.download_throttle()->set_list_iterator(m_download->download_throttle()->end());
  m_peerChunks.download_throttle()->slot_activate(rak::make_mem_fun(this, &PeerConnectionBase::receive_throttle_down_activate));

  download_queue()->set_delegator(m_download->delegator());
  download_queue()->set_peer_chunks(&m_peerChunks);

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

  initialize_custom();
}

void
PeerConnectionBase::cleanup() {
  if (!get_fd().is_valid())
    return;

  if (m_download == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  m_downloadQueue.clear();

  up_chunk_release();
  down_chunk_release();

  m_download->upload_choke_manager()->disconnected(this, &m_upChoke);
  m_download->download_choke_manager()->disconnected(this, &m_downChoke);
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

  m_download->upload_throttle()->erase(m_peerChunks.upload_throttle());
  m_download->download_throttle()->erase(m_peerChunks.download_throttle());

  m_up->set_state(ProtocolWrite::INTERNAL_ERROR);
  m_down->set_state(ProtocolRead::INTERNAL_ERROR);

  m_download = NULL;
}

void
PeerConnectionBase::set_upload_snubbed(bool v) {
  if (v)
    m_download->upload_choke_manager()->set_snubbed(this, &m_upChoke);
  else
    m_download->upload_choke_manager()->set_not_snubbed(this, &m_upChoke);
}

bool
PeerConnectionBase::receive_upload_choke(bool choke) {
  if (choke == m_upChoke.choked())
    throw internal_error("PeerConnectionBase::receive_upload_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_sendChoked = true;
  m_upChoke.set_unchoked(!choke);
  m_upChoke.set_time_last_choke(cachedTime);

  return true;
}

bool
PeerConnectionBase::receive_download_choke(bool choke) {
  if (choke == m_downChoke.choked())
    throw internal_error("PeerConnectionBase::receive_download_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_downChoke.set_unchoked(!choke);
  m_downChoke.set_time_last_choke(cachedTime);

  if (choke) {
    m_peerChunks.download_cache()->disable();

    // If the queue isn't empty, then we might still receive some
    // pieces, so don't remove us from throttle.
    if (!download_queue()->is_downloading() && download_queue()->empty())
      m_download->download_throttle()->erase(m_peerChunks.download_throttle());

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
      m_downChoke.set_queued(false);
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
    return;
  }

  up_chunk_release();
  
  m_upChunk = m_download->chunk_list()->get(m_upPiece.index(), false);
  
  if (!m_upChunk.is_valid())
    throw storage_error("File chunk read error: " + std::string(m_upChunk.error_number().c_str()));

  if (is_encrypted() && m_encryptBuffer == NULL) {
    m_encryptBuffer = new EncryptBuffer();
    m_encryptBuffer->reset();
  }

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
    throw internal_error("PeerConnectionBase::cancel_transfer(...) !get_fd().is_valid().");

  // We don't send cancel messages if the transfer has already
  // started.
  if (transfer == m_downloadQueue.transfer())
    return;

  write_insert_poll_safe();

  m_peerChunks.cancel_queue()->push_back(transfer->piece());
//   m_downloadQueue.cancel_transfer(transfer);
}

void
PeerConnectionBase::receive_throttle_down_activate() {
  manager->poll()->insert_read(this);
}

void
PeerConnectionBase::receive_throttle_up_activate() {
  manager->poll()->insert_write(this);
}

void
PeerConnectionBase::event_error() {
  m_download->connection_list()->erase(this, 0);
}

bool
PeerConnectionBase::down_chunk_start(const Piece& piece) {
  if (!download_queue()->downloading(piece)) {
    if (piece.length() == 0)
      m_download->info()->signal_network_log().emit("Received piece with length zero.");

    return false;
  }

  if (!m_download->file_list()->is_valid_piece(piece))
    throw internal_error("Incoming pieces list contains a bad piece.");
  
  if (!m_downChunk.is_valid() || piece.index() != m_downChunk.index()) {
    down_chunk_release();
    m_downChunk = m_download->chunk_list()->get(piece.index(), true);
  
    if (!m_downChunk.is_valid())
      throw storage_error("File chunk write error: " + std::string(m_downChunk.error_number().c_str()) + ".");
  }

  return m_downloadQueue.transfer()->is_leader();
}

void
PeerConnectionBase::down_chunk_finished() {
  if (!download_queue()->transfer()->is_finished())
    throw internal_error("PeerConnectionBase::down_chunk_finished() Transfer not finished.");

  if (download_queue()->transfer()->is_leader()) {
    if (!m_downChunk.is_valid())
      throw internal_error("PeerConnectionBase::down_chunk_finished() Transfer is the leader, but no chunk allocated.");

    download_queue()->finished();
    m_downChunk.object()->set_time_modified(cachedTime);

  } else {
    download_queue()->skipped();
  }
        
  if (m_downStall > 0)
    m_downStall--;
        
  // TODO: clear m_down.data?

  // If we were choked by choke_manager but still had queued pieces,
  // then we might still be in the throttle.
  if (m_downChoke.choked() && download_queue()->empty())
    m_download->download_throttle()->erase(m_peerChunks.download_throttle());

  write_insert_poll_safe();
}

bool
PeerConnectionBase::down_chunk() {
  if (!m_download->download_throttle()->is_throttled(m_peerChunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk() tried to read a piece but is not in throttle list");

  if (!m_downChunk.chunk()->is_writable())
    throw internal_error("PeerConnectionBase::down_part() chunk not writable, permission denided");

  uint32_t quota = m_download->download_throttle()->node_quota(m_peerChunks.download_throttle());

  if (quota == 0) {
    manager->poll()->remove_read(this);
    m_download->download_throttle()->node_deactivate(m_peerChunks.download_throttle());
    return false;
  }

  uint32_t bytesTransfered = 0;
  BlockTransfer* transfer = m_downloadQueue.transfer();

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

  } while (itr.used(data.second));

  transfer->adjust_position(bytesTransfered);

  m_download->download_throttle()->node_used(m_peerChunks.download_throttle(), bytesTransfered);
  m_download->info()->down_rate()->insert(bytesTransfered);

  return transfer->is_finished();
}

bool
PeerConnectionBase::down_chunk_from_buffer() {
  m_down->buffer()->consume(down_chunk_process(m_down->buffer()->position(), m_down->buffer()->remaining()));

  if (!m_downloadQueue.transfer()->is_finished() && m_down->buffer()->remaining() != 0)
    throw internal_error("PeerConnectionBase::down_chunk_from_buffer() !transfer->is_finished() && m_down->buffer()->remaining() != 0.");

  return m_downloadQueue.transfer()->is_finished();
}  

// When this transfer again becomes the leader, we just return false
// and wait for the next polling. It is an exceptional case so we
// don't really care that much about performance.
bool
PeerConnectionBase::down_chunk_skip() {
  ThrottleList* throttle = m_download->download_throttle();

  if (!throttle->is_throttled(m_peerChunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk_skip() tried to read a piece but is not in throttle list");

  uint32_t quota = throttle->node_quota(m_peerChunks.download_throttle());

  if (quota == 0) {
    manager->poll()->remove_read(this);
    throttle->node_deactivate(m_peerChunks.download_throttle());
    return false;
  }

  uint32_t length = read_stream_throws(m_nullBuffer, std::min(quota, m_downloadQueue.transfer()->piece().length() - m_downloadQueue.transfer()->position()));
  throttle->node_used(m_peerChunks.download_throttle(), length);

  if (is_encrypted())
    m_encryption.decrypt(m_nullBuffer, length);

  if (down_chunk_skip_process(m_nullBuffer, length) != length)
    throw internal_error("PeerConnectionBase::down_chunk_skip() down_chunk_skip_process(m_nullBuffer, length) != length.");

  return m_downloadQueue.transfer()->is_finished();
}

bool
PeerConnectionBase::down_chunk_skip_from_buffer() {
  m_down->buffer()->consume(down_chunk_skip_process(m_down->buffer()->position(), m_down->buffer()->remaining()));
  
  return m_downloadQueue.transfer()->is_finished();
}

// Process data from a leading transfer.
uint32_t
PeerConnectionBase::down_chunk_process(const void* buffer, uint32_t length) {
  if (!m_downChunk.is_valid() || m_downChunk.index() != m_downloadQueue.transfer()->index())
    throw internal_error("PeerConnectionBase::down_chunk_process(...) !m_downChunk.is_valid() || m_downChunk.index() != m_downloadQueue.transfer()->index().");

  if (length == 0)
    return length;

  BlockTransfer* transfer = m_downloadQueue.transfer();

  length = std::min(transfer->piece().length() - transfer->position(), length);

  m_downChunk.chunk()->from_buffer(buffer, transfer->piece().offset() + transfer->position(), length);

  transfer->adjust_position(length);

  m_download->download_throttle()->node_used(m_peerChunks.download_throttle(), length);
  m_download->info()->down_rate()->insert(length);

  return length;
}

// Process data from non-leading transfer. If this transfer encounters
// mismatching data with the leader then bork this transfer. If we get
// ahead of the leader, we switch the leader.
uint32_t
PeerConnectionBase::down_chunk_skip_process(const void* buffer, uint32_t length) {
  BlockTransfer* transfer = m_downloadQueue.transfer();

  // Adjust 'length' to be less than or equal to what is remaining of
  // the block to simplify the rest of the function.
  length = std::min(length, transfer->piece().length() - transfer->position());

  // Hmm, this might result in more bytes than nessesary being
  // counted.
  m_download->download_throttle()->node_used(m_peerChunks.download_throttle(), length);
  m_download->info()->down_rate()->insert(length);
  m_download->info()->skip_rate()->insert(length);

  if (!transfer->is_valid()) {
    transfer->adjust_position(length);
    return length;
  }

  if (!transfer->block()->is_transfering())
    throw internal_error("PeerConnectionBase::down_chunk_skip_process(...) block is not transfering, yet we have non-leaders.");

  // Temporary test.
  if (transfer->position() > transfer->block()->leader()->position())
    throw internal_error("PeerConnectionBase::down_chunk_skip_process(...) transfer is past the Block's position.");

  // If the transfer is valid, compare the downloaded data to the
  // leader.
  uint32_t compareLength = std::min(length, transfer->block()->leader()->position() - transfer->position());

  // The data doesn't match with what has previously been downloaded,
  // bork this transfer.
  if (!m_downChunk.chunk()->compare_buffer(buffer, transfer->piece().offset() + transfer->position(), compareLength)) {
    m_download->info()->signal_network_log().emit("Data does not match what was previously downloaded.");
    
    m_downloadQueue.transfer_dissimilar();
    m_downloadQueue.transfer()->adjust_position(length);

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
  uint32_t need = m_extensions->read_need();

  // If we need more bytes, read as much as we can get nonetheless.
  if (need > m_down->buffer()->remaining() && m_down->buffer()->reserved_left() > 0) {
    uint32_t read = m_down->buffer()->move_end(read_stream_throws(m_down->buffer()->end(), m_down->buffer()->reserved_left()));
    m_download->download_throttle()->node_used_unthrottled(read);

    if (is_encrypted())
      m_encryption.decrypt(m_down->buffer()->end() - read, read);
  }

  if (m_down->buffer()->consume(m_extensions->read(m_down->buffer()->position(), std::min<uint32_t>(m_down->buffer()->remaining(), need))))
    m_down->buffer()->reset();

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
  if (!m_download->upload_throttle()->is_throttled(m_peerChunks.upload_throttle()))
    throw internal_error("PeerConnectionBase::up_chunk() tried to write a piece but is not in throttle list");

  if (!m_upChunk.chunk()->is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  uint32_t quota = m_download->upload_throttle()->node_quota(m_peerChunks.upload_throttle());

  if (quota == 0) {
    manager->poll()->remove_write(this);
    m_download->upload_throttle()->node_deactivate(m_peerChunks.upload_throttle());
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

    } while (itr.used(data.second));
  }

  m_download->upload_throttle()->node_used(m_peerChunks.upload_throttle(), bytesTransfered);
  m_download->info()->up_rate()->insert(bytesTransfered);

  // Just modifying the piece to cover the remaining data ends up
  // being much cleaner and we avoid an unnessesary position variable.
  m_upPiece.set_offset(m_upPiece.offset() + bytesTransfered);
  m_upPiece.set_length(m_upPiece.length() - bytesTransfered);

  return m_upPiece.length() == 0;
}

bool
PeerConnectionBase::up_extension() {
  if (m_extensionOffset == extension_must_encrypt) {
    if (m_extensionMessage.copied()) {
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
  m_download->upload_throttle()->node_used_unthrottled(written);
  m_extensionOffset += written;

  if (m_extensionOffset < m_extensionMessage.length())
    return false;

  // clear() deletes the buffer, only do that if we made a copy,
  // otherwise the buffer is shared among all connections.
  if (m_extensionMessage.copied())
    m_extensionMessage.clear();
  else 
    m_extensionMessage.set(NULL, NULL, false);

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
  PeerChunks::piece_list_type::iterator itr = std::find(m_peerChunks.upload_queue()->begin(), m_peerChunks.upload_queue()->end(), p);
  
  if (m_upChoke.choked() || itr != m_peerChunks.upload_queue()->end() || p.length() > (1 << 17))
    return;

  m_peerChunks.upload_queue()->push_back(p);
  write_insert_poll_safe();
}

void
PeerConnectionBase::read_cancel_piece(const Piece& p) {
  PeerChunks::piece_list_type::iterator itr = std::find(m_peerChunks.upload_queue()->begin(), m_peerChunks.upload_queue()->end(), p);
  
  if (itr != m_peerChunks.upload_queue()->end())
    m_peerChunks.upload_queue()->erase(itr);
}  

void
PeerConnectionBase::write_prepare_piece() {
  m_upPiece = m_peerChunks.upload_queue()->front();
  m_peerChunks.upload_queue()->pop_front();

  // Move these checks somewhere else?
  if (!m_download->file_list()->is_valid_piece(m_upPiece) ||
      !m_download->file_list()->bitfield()->get(m_upPiece.index())) {
    char buffer[128];
    snprintf(buffer, 128, "Peer requested an invalid piece: %u %u %u", m_upPiece.index(), m_upPiece.length(), m_upPiece.offset());

    throw communication_error(buffer);
  }
  
  m_up->write_piece(m_upPiece);
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
  if (download_queue()->empty())
    m_downStall = 0;

  uint32_t pipeSize = download_queue()->calculate_pipe_size(m_peerChunks.download_throttle()->rate()->rate());

  // Don't start requesting if we can't do it in large enough chunks.
  if (download_queue()->size() >= (pipeSize + 10) / 2)
    return false;

  bool success = false;

  while (download_queue()->size() < pipeSize && m_up->can_write_request()) {

    // Delegator should return a vector of pieces, and it should be
    // passed the number of pieces it should delegate. Try to ensure
    // it receives large enough request to fill a whole chunk if the
    // peer is fast enough.

    const Piece* p = download_queue()->delegate();

    if (p == NULL)
      break;

    if (!m_download->file_list()->is_valid_piece(*p) || !m_peerChunks.bitfield()->get(p->index()))
      throw internal_error("PeerConnectionBase::try_request_pieces() tried to use an invalid piece.");

    m_up->write_request(*p);

    success = true;
  }

  return success;
}

// Send one peer exchange message according to bits set in m_sendPEXMask.
// We can only send one message at a time, because the derived class
// needs to flush the buffer and call up_extension before the next one.
void
PeerConnectionBase::send_pex_message() {
  // Message to tell peer to stop/start doing PEX is small so send it first.
  if (m_sendPEXMask & (PEX_ENABLE | PEX_DISABLE)) {
    write_prepare_extension(ProtocolExtension::HANDSHAKE,
                            ProtocolExtension::generate_toggle_message(ProtocolExtension::UT_PEX, m_sendPEXMask & PEX_ENABLE != 0));

    m_sendPEXMask &= ~(PEX_ENABLE | PEX_DISABLE);

  } else if (m_sendPEXMask & PEX_DO) {
    const DataBuffer& pexMessage = m_download->get_ut_pex(m_extensions->is_initial_pex());
    m_extensions->clear_initial_pex();

    if (!pexMessage.empty())
      write_prepare_extension(ProtocolExtension::UT_PEX, pexMessage);

    m_sendPEXMask &= ~PEX_DO;

  } else {
    throw internal_error("ProtocolConnectionBase::send_pex_message invalid value in m_sendPEXMask.");
  }
}

}
