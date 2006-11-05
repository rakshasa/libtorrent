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

#include <sstream>
#include <rak/error_number.h>

#include "torrent/exceptions.h"
#include "torrent/block.h"
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
#include "torrent/peer_info.h"

#include "peer_connection_base.h"

#include "manager.h"

namespace torrent {

PeerConnectionBase::PeerConnectionBase() :
  m_download(NULL),
  
  m_down(new ProtocolRead()),
  m_up(new ProtocolWrite()),

  m_peerInfo(NULL),

  m_downStall(0),

  m_sendChoked(false),
  m_sendInterested(false),

  m_encryptBuffer(NULL) {
}

PeerConnectionBase::~PeerConnectionBase() {
  delete m_up;
  delete m_down;

  delete m_encryptBuffer;
}

void
PeerConnectionBase::initialize(DownloadMain* download, PeerInfo* peerInfo, SocketFd fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo) {
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
  update_interested();

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

  m_download->choke_manager()->disconnected(this);
  m_download->chunk_statistics()->received_disconnect(&m_peerChunks);

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
PeerConnectionBase::set_snubbed(bool v) {
  if (v == m_peerChunks.is_snubbed())
    return;

  bool wasUploadWanted = is_upload_wanted();
  m_peerChunks.set_snubbed(v);

  if (v) {
    if (wasUploadWanted)
      m_download->choke_manager()->set_not_interested(this);

  } else {
    if (is_upload_wanted())
      m_download->choke_manager()->set_interested(this);
  }
}

void
PeerConnectionBase::receive_choke(bool v) {
  if (v == m_up->choked())
    throw internal_error("PeerConnectionBase::receive_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_sendChoked = true;
  m_up->set_choked(v);
  m_timeLastChoked = cachedTime;
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

  if (!m_download->content()->is_valid_piece(piece))
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
  // TODO: remove throttle if choked? Rarely happens though.
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
  uint32_t length = read_stream_throws(m_nullBuffer, m_downloadQueue.transfer()->piece().length() - m_downloadQueue.transfer()->position());

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
  
  if (m_up->choked() || itr != m_peerChunks.upload_queue()->end() || p.length() > (1 << 17))
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
PeerConnectionBase::read_buffer_move_unused() {
  uint32_t remaining = m_down->buffer()->remaining();
  
  std::memmove(m_down->buffer()->begin(), m_down->buffer()->position(), remaining);
  
  m_down->buffer()->reset_position();
  m_down->buffer()->set_end(remaining);
}

void
PeerConnectionBase::write_prepare_piece() {
  m_upPiece = m_peerChunks.upload_queue()->front();
  m_peerChunks.upload_queue()->pop_front();

  // Move these checks somewhere else?
  if (!m_download->content()->is_valid_piece(m_upPiece) ||
      !m_download->content()->has_chunk(m_upPiece.index())) {
    std::stringstream s;

    s << "Peer requested a piece with invalid index or length/offset: "
      << m_upPiece.index() << ' '
      << m_upPiece.length() << ' '
      << m_upPiece.offset();

    throw communication_error(s.str());
//     throw communication_error("Peer requested a piece with invalid index or length/offset.");
  }
      
  m_up->write_piece(m_upPiece);
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
PeerConnectionBase::should_request() {
  if (m_down->choked() || !m_up->interested())
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

    if (!m_download->content()->is_valid_piece(*p) || !m_peerChunks.bitfield()->get(p->index()))
      throw internal_error("PeerConnectionBase::try_request_pieces() tried to use an invalid piece.");

    m_up->write_request(*p);

    success = true;
  }

  return success;
}

void
PeerConnectionBase::set_remote_interested() {
  if (m_down->interested() || m_peerChunks.bitfield()->is_all_set())
    return;

  m_down->set_interested(true);

  if (is_upload_wanted())
    m_download->choke_manager()->set_interested(this);
}

void
PeerConnectionBase::set_remote_not_interested() {
  if (!m_down->interested())
    return;

  bool wasUploadWanted = is_upload_wanted();

  m_down->set_interested(false);

  if (wasUploadWanted)
    m_download->choke_manager()->set_not_interested(this);
}

}
