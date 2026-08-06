#include "config.h"

#include "protocol/peer_connection_base.h"

#include <cstdio>

#include "data/chunk_iterator.h"
#include "data/chunk_list.h"
#include "download/chunk_statistics.h"
#include "download/download_main.h"
#include "protocol/encryption_info.h"
#include "protocol/extensions.h"
#include "torrent/data/block.h"
#include "torrent/download/choke_group.h"
#include "torrent/download_info.h"
#include "torrent/net/fd.h"
#include "torrent/peer/connection_list.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/runtime/memory_manager.h"
#include "torrent/utils/log.h"

#define LT_LOG_PIECE_EVENTS(log_fmt, ...)                               \
  lt_log_print_info(LOG_PROTOCOL_PIECE_EVENTS, this->download()->info(), "piece_events", "%40s " log_fmt, this->peer_info()->id_hex(), __VA_ARGS__);


namespace torrent {

static void
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
  m_down(new ProtocolRead()),
  m_up(new ProtocolWrite()) {

  m_peerInfo = nullptr;
}

PeerConnectionBase::~PeerConnectionBase() {
  delete m_up;
  delete m_down;

  if (m_extensions != NULL && !m_extensions->is_default())
    delete m_extensions;

  m_extension_message.clear();
}

void
PeerConnectionBase::initialize(DownloadMain* download, PeerInfo* peerInfo, int fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions) {
  if (is_open())
    throw internal_error("Tried to re-set PeerConnection.");

  if (fd < 0)
    throw internal_error("PeerConnectionBase::initialize() received invalid fd.");

  if (encryptionInfo->is_encrypted() != encryptionInfo->decrypt_valid())
    throw internal_error("Encryption and decryption inconsistent.");

  set_file_descriptor(fd);

  m_peerInfo = peerInfo;
  m_download = download;

  m_encryption = *encryptionInfo;
  m_extensions = extensions;

  m_extensions->set_connection(this);

  m_up_choke.set_entry(m_download->up_group_entry());
  m_down_choke.set_entry(m_download->down_group_entry());

  m_peer_chunks.set_peer_info(m_peerInfo);
  m_peer_chunks.bitfield()->swap(*bitfield);

  m_up->set_throttle(m_download->upload_throttle());
  m_down->set_throttle(m_download->download_throttle());

  m_peer_chunks.upload_throttle()->set_list_iterator(m_up->throttle()->end());
  m_peer_chunks.upload_throttle()->slot_activate() = [this] { this_thread::poll()->insert_write(this); };

  m_peer_chunks.download_throttle()->set_list_iterator(m_down->throttle()->end());
  m_peer_chunks.download_throttle()->slot_activate() = [this] { this_thread::poll()->insert_read(this); };

  request_list()->set_delegator(m_download->delegator());
  request_list()->set_peer_chunks(&m_peer_chunks);

  try {
    initialize_custom();

  } catch (const close_connection&) {
    // The handshake manager closes the socket for us.
    m_peerInfo   = nullptr;
    m_download   = nullptr;
    m_extensions = nullptr;

    reset_file_descriptor();
    return;
  }

  this_thread::poll()->open(this);
  this_thread::poll()->insert_read(this);
  this_thread::poll()->insert_write(this);

  m_time_last_read = this_thread::cached_time();

  m_download->chunk_statistics()->received_connect(&m_peer_chunks);

  // Hmm... cleanup?
//   update_interested();

  m_peer_chunks.download_cache()->clear();

  if (!m_download->file_list()->is_done()) {
    m_send_interested = true;
    m_down_interested = true;
  }
}

void
PeerConnectionBase::cleanup() {
  if (!is_open())
    return;

  if (m_download == NULL)
    throw internal_error("PeerConnection::~PeerConnection() m_fd is valid but m_state and/or m_net is NULL");

  // TODO: Verify that transfer counter gets modified by this...
  m_request_list.clear();

  up_chunk_release();
  down_chunk_release();

  m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() - m_up_choke.unchoked());
  m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() - m_down_choke.unchoked());

  m_download->choke_group()->up_queue()->disconnected(this, &m_up_choke);
  m_download->choke_group()->down_queue()->disconnected(this, &m_down_choke);
  m_download->chunk_statistics()->received_disconnect(&m_peer_chunks);

  if (!m_extensions->is_default())
    m_extensions->cleanup();

  runtime::socket_manager()->close_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close(file_descriptor());
      reset_file_descriptor();
    });

  m_up->throttle()->erase(m_peer_chunks.upload_throttle());
  m_down->throttle()->erase(m_peer_chunks.download_throttle());

  m_up->set_state(ProtocolWrite::INTERNAL_ERROR);
  m_down->set_state(ProtocolRead::INTERNAL_ERROR);

  m_download = NULL;
}

void
PeerConnectionBase::set_peer_exchange(bool state) {
  if (m_extensions->is_default() || !m_extensions->is_remote_supported(ProtocolExtension::UT_PEX))
    return;

  if (state) {
    m_send_pex_mask = PEX_ENABLE | (m_send_pex_mask & ~PEX_DISABLE);
    m_extensions->set_local_enabled(ProtocolExtension::UT_PEX);
  } else {
    m_send_pex_mask = PEX_DISABLE | (m_send_pex_mask & ~PEX_ENABLE);
    m_extensions->unset_local_enabled(ProtocolExtension::UT_PEX);
  }
}

void
PeerConnectionBase::set_upload_snubbed(bool v) {
  if (v)
    m_download->choke_group()->up_queue()->set_snubbed(this, &m_up_choke);
  else
    m_download->choke_group()->up_queue()->set_not_snubbed(this, &m_up_choke);
}

bool
PeerConnectionBase::receive_upload_choke(bool choke) {
  if (choke == m_up_choke.choked())
    throw internal_error("PeerConnectionBase::receive_upload_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_send_choked = true;
  m_up_choke.set_unchoked(!choke);
  m_up_choke.set_time_last_choke(this_thread::cached_time());

  if (choke) {
    m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() - 1);
    m_up_choke.entry()->connection_choked(this);
    m_up_choke.entry()->connection_queued(this);

    m_download->choke_group()->up_queue()->modify_currently_unchoked(-1);
    m_download->choke_group()->up_queue()->modify_currently_queued(1);

  } else {
    m_download->info()->set_upload_unchoked(m_download->info()->upload_unchoked() + 1);
    m_up_choke.entry()->connection_unqueued(this);
    m_up_choke.entry()->connection_unchoked(this);

    m_download->choke_group()->up_queue()->modify_currently_unchoked(1);
    m_download->choke_group()->up_queue()->modify_currently_queued(-1);
  }

  return true;
}

bool
PeerConnectionBase::receive_download_choke(bool choke) {
  if (choke == m_down_choke.choked())
    throw internal_error("PeerConnectionBase::receive_download_choke(...) already set to the same state.");

  write_insert_poll_safe();

  m_down_choke.set_unchoked(!choke);
  m_down_choke.set_time_last_choke(this_thread::cached_time());

  if (choke) {
    m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() - 1);
    m_down_choke.entry()->connection_choked(this);
    m_down_choke.entry()->connection_queued(this);

    m_download->choke_group()->down_queue()->modify_currently_unchoked(-1);
    m_download->choke_group()->down_queue()->modify_currently_queued(1);

  } else {
    m_download->info()->set_download_unchoked(m_download->info()->download_unchoked() + 1);
    m_down_choke.entry()->connection_unqueued(this);
    m_down_choke.entry()->connection_unchoked(this);

    m_download->choke_group()->down_queue()->modify_currently_unchoked(1);
    m_download->choke_group()->down_queue()->modify_currently_queued(-1);
  }

  if (choke) {
    m_peer_chunks.download_cache()->disable();

    // If the queue isn't empty, then we might still receive some
    // pieces, so don't remove us from throttle or release the chunk.
    if (!request_list()->is_downloading() && request_list()->queued_empty()) {
      m_down->throttle()->erase(m_peer_chunks.download_throttle());
      down_chunk_release();
    }

    // Send uninterested if unchoked, but only _after_ receiving our
    // chunks?

    if (m_down_unchoked) {
      // Tell the peer we're no longer interested to avoid
      // disconnects. We keep the connection in the queue so that
      // ChokeManager::cycle(...) can attempt to get us unchoked
      // again.

      m_send_interested = m_down_interested;
      m_down_interested = false;

    } else {
      // Remove from queue so that an unchoke from the remote peer
      // will cause the connection to be unchoked immediately by the
      // choke manager.
      //
      // TODO: This doesn't seem safe...
      m_download->choke_group()->down_queue()->set_not_queued(this, &m_down_choke);
      return false;
    }

  } else {
    m_tryRequest = true;

    if (!m_down_interested) {
      // We were marked as not interested by the cycling choke and
      // kept in the queue, thus the peer should have some pieces of
      // interest.
      //
      // We have now been 'unchoked' by the choke manager, so tell the
      // peer that we're again interested. If the peer doesn't unchoke
      // us within a cycle or two we're likely to be choked and left
      // out of the queue. So if the peer unchokes us at a later time,
      // we skip the queue and unchoke immediately.

      m_send_interested = !m_down_interested;
      m_down_interested = true;
    }
  }

  return true;
}

void
PeerConnectionBase::load_up_chunk() {
  if (m_up_chunk.is_valid() && m_up_chunk.index() == m_up_piece.index()) {
    // Better checking needed.
    //     m_up_chunk.chunk()->preload(m_up_piece.offset(), m_up_chunk.chunk()->size());

    if (lt_log_is_valid(LOG_INSTRUMENTATION_MINCORE))
      log_mincore_stats_func(m_up_chunk.chunk()->is_incore(m_up_piece.offset(), m_up_piece.length()), false, m_incore_continous);

    return;
  }

  up_chunk_release();

  m_up_chunk = m_download->chunk_list()->get(m_up_piece.index(), ChunkList::get_not_hashing);

  if (!m_up_chunk.is_valid())
    throw storage_error("File chunk read error: " + std::string(std::strerror(m_up_chunk.error_number())));

  if (is_encrypted() && m_encrypt_buffer == nullptr) {
    m_encrypt_buffer = std::make_unique<EncryptBuffer>();
    m_encrypt_buffer->reset();
  }

  m_incore_continous = false;

  if (lt_log_is_valid(LOG_INSTRUMENTATION_MINCORE))
    log_mincore_stats_func(m_up_chunk.chunk()->is_incore(m_up_piece.offset(), m_up_piece.length()), true, m_incore_continous);

  m_incore_continous = true;

  // Also check if we've already preloaded in the recent past, even
  // past unmaps.
  auto* memory_manager = runtime::memory_manager();
  auto  preload_type   = memory_manager->preload_type();
  auto  preload_size   = m_up_chunk.chunk()->chunk_size() - m_up_piece.offset();

  if (preload_type == 0 || preload_size < memory_manager->preload_min_size())
    return;

  if (m_up_chunk.object()->time_preloaded() + 60s >= this_thread::cached_time()) {
    memory_manager->increment_stats_not_preloaded();
    return;
  }

  auto preload_size_mb = (preload_size + (1 << 20) - 1) / (1 << 20);

  if (m_peer_chunks.upload_throttle()->rate()->rate() < memory_manager->preload_required_rate() * preload_size_mb) {
    memory_manager->increment_stats_not_preloaded();
    return;
  }

  memory_manager->increment_stats_preloaded();

  m_up_chunk.object()->set_time_preloaded(this_thread::cached_time());
  m_up_chunk.chunk()->preload(m_up_piece.offset(), m_up_chunk.chunk()->chunk_size(), (preload_type == 1));
}

void
PeerConnectionBase::cancel_transfer(BlockTransfer* transfer) {
  if (!is_open())
    throw internal_error("PeerConnectionBase::cancel_transfer(...) !is_open()");

  if (transfer->peer_info() != peer_info())
    throw internal_error("PeerConnectionBase::cancel_transfer(...) peer info doesn't match");

  // We don't send cancel messages if the transfer has already
  // started.
  if (transfer == m_request_list.transfer())
    return;

  write_insert_poll_safe();

  m_peer_chunks.cancel_queue()->push_back(transfer->piece());
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

  if (!m_down_chunk.is_valid() || piece.index() != m_down_chunk.index()) {
    down_chunk_release();
    m_down_chunk = m_download->chunk_list()->get(piece.index(), ChunkList::get_not_hashing | ChunkList::get_writable);

    if (!m_down_chunk.is_valid())
      throw storage_error("File chunk write error: " + std::string(std::strerror(m_down_chunk.error_number())));
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
    if (!m_down_chunk.is_valid())
      throw internal_error("PeerConnectionBase::down_chunk_finished() Transfer is the leader, but no chunk allocated.");

    request_list()->finished();
    m_down_chunk.object()->set_time_modified(this_thread::cached_time());

  } else {
    request_list()->skipped();
  }

  if (m_down_stall > 0)
    m_down_stall--;

  // We need to release chunks when we're not sure if they will be
  // used in the near future so as to avoid hitting the address space
  // limit in high-bandwidth situations.
  //
  // Some tweaking of the pipe size might be necessary if the queue
  // empties too often.
  if (m_down_chunk.is_valid() &&
      (request_list()->queued_empty() || m_down_chunk.index() != request_list()->next_queued_piece().index()))
    down_chunk_release();

  // If we were choked by choke_manager but still had queued pieces,
  // then we might still be in the throttle.
  if (m_down_choke.choked() && request_list()->queued_empty())
    m_down->throttle()->erase(m_peer_chunks.download_throttle());

  write_insert_poll_safe();
}

bool
PeerConnectionBase::down_chunk() {
  if (!m_down->throttle()->is_throttled(m_peer_chunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk() tried to read a piece but is not in throttle list");

  if (!m_down_chunk.chunk()->is_writable())
    throw internal_error("PeerConnectionBase::down_part() chunk not writable, permission denided");

  uint32_t quota = m_down->throttle()->node_quota(m_peer_chunks.download_throttle());

  if (quota == 0) {
    this_thread::poll()->remove_read(this);
    m_down->throttle()->node_deactivate(m_peer_chunks.download_throttle());
    return false;
  }

  uint32_t bytesTransfered = 0;
  BlockTransfer* transfer = m_request_list.transfer();

  Chunk::data_type data;
  ChunkIterator itr(m_down_chunk.chunk(),
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

  m_down->throttle()->node_used(m_peer_chunks.download_throttle(), bytesTransfered);
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

  if (!throttle->is_throttled(m_peer_chunks.download_throttle()))
    throw internal_error("PeerConnectionBase::down_chunk_skip() tried to read a piece but is not in throttle list");

  uint32_t quota = throttle->node_quota(m_peer_chunks.download_throttle());

  if (quota == 0) {
    this_thread::poll()->remove_read(this);
    throttle->node_deactivate(m_peer_chunks.download_throttle());
    return false;
  }

  uint32_t length = read_stream_throws(m_nullBuffer, std::min(quota, m_request_list.transfer()->piece().length() - m_request_list.transfer()->position()));
  throttle->node_used(m_peer_chunks.download_throttle(), length);

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
  if (!m_down_chunk.is_valid() || m_down_chunk.index() != m_request_list.transfer()->index())
    throw internal_error("PeerConnectionBase::down_chunk_process(...) !m_down_chunk.is_valid() || m_down_chunk.index() != m_request_list.transfer()->index().");

  if (length == 0)
    return length;

  BlockTransfer* transfer = m_request_list.transfer();

  length = std::min(transfer->piece().length() - transfer->position(), length);

  m_down_chunk.chunk()->from_buffer(buffer, transfer->piece().offset() + transfer->position(), length);

  transfer->adjust_position(length);

  m_down->throttle()->node_used(m_peer_chunks.download_throttle(), length);
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
  m_down->throttle()->node_used(m_peer_chunks.download_throttle(), length);
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
  if (!m_down_chunk.chunk()->compare_buffer(buffer, transfer->piece().offset() + transfer->position(), compareLength)) {
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
    uint32_t need = std::min(m_extensions->read_need(), static_cast<uint32_t>(m_down->buffer()->remaining()));
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
    this_thread::poll()->remove_read(this);
    return false;
  }

  return m_extensions->is_complete();
}

inline uint32_t
PeerConnectionBase::up_chunk_encrypt(uint32_t quota) {
  if (m_encrypt_buffer == nullptr)
    throw internal_error("PeerConnectionBase::up_chunk: m_encrypt_buffer is NULL.");

  if (quota <= m_encrypt_buffer->remaining())
    return quota;

  // Also, consider checking here if the number of bytes remaining in
  // the buffer is small enought that the cost of moving them would
  // outweigh the extra context switches, etc.

  if (m_encrypt_buffer->remaining() == 0) {
    // This handles reset also for new chunk transfers.
    m_encrypt_buffer->reset();

    quota = std::min<uint32_t>(quota, m_encrypt_buffer->reserved());

  } else {
    quota = std::min<uint32_t>(quota - m_encrypt_buffer->remaining(), m_encrypt_buffer->reserved_left());
  }

  m_up_chunk.chunk()->to_buffer(m_encrypt_buffer->end(), m_up_piece.offset() + m_encrypt_buffer->remaining(), quota);
  m_encryption.encrypt(m_encrypt_buffer->end(), quota);
  m_encrypt_buffer->move_end(quota);

  return m_encrypt_buffer->remaining();
}

bool
PeerConnectionBase::up_chunk() {
  if (!m_up->throttle()->is_throttled(m_peer_chunks.upload_throttle()))
    throw internal_error("PeerConnectionBase::up_chunk() tried to write a piece but is not in throttle list");

  if (!m_up_chunk.chunk()->is_readable())
    throw internal_error("ProtocolChunk::write_part() chunk not readable, permission denided");

  uint32_t quota = m_up->throttle()->node_quota(m_peer_chunks.upload_throttle());

  if (quota == 0) {
    this_thread::poll()->remove_write(this);
    m_up->throttle()->node_deactivate(m_peer_chunks.upload_throttle());
    return false;
  }

  uint32_t bytesTransfered = 0;

  if (is_encrypted()) {
    // Prepare as many bytes as quota specifies, up to end of piece or
    // buffer. Only bytes beyond remaining() are new and will be
    // encrypted.
    quota = up_chunk_encrypt(std::min(quota, m_up_piece.length()));

    bytesTransfered = write_stream_throws(m_encrypt_buffer->position(), quota);
    m_encrypt_buffer->consume(bytesTransfered);

  } else {
    Chunk::data_type data;
    ChunkIterator itr(m_up_chunk.chunk(), m_up_piece.offset(), m_up_piece.offset() + std::min(quota, m_up_piece.length()));

    do {
      data = itr.data();
      data.second = write_stream_throws(data.first, data.second);

      bytesTransfered += data.second;

    } while (data.second != 0 && itr.forward(data.second));
  }

  m_up->throttle()->node_used(m_peer_chunks.upload_throttle(), bytesTransfered);
  m_download->info()->mutable_up_rate()->insert(bytesTransfered);

  // Just modifying the piece to cover the remaining data ends up
  // being much cleaner and we avoid an unnessesary position variable.
  m_up_piece.set_offset(m_up_piece.offset() + bytesTransfered);
  m_up_piece.set_length(m_up_piece.length() - bytesTransfered);

  return m_up_piece.length() == 0;
}

bool
PeerConnectionBase::up_extension() {
  if (m_extension_offset == extension_must_encrypt) {
    if (m_extension_message.owned()) {
      m_encryption.encrypt(m_extension_message.data(), m_extension_message.length());

    } else {
      auto buffer = new char[m_extension_message.length()];

      m_encryption.encrypt(m_extension_message.data(), buffer, m_extension_message.length());
      m_extension_message.set(buffer, buffer + m_extension_message.length(), true);
    }

    m_extension_offset = 0;
  }

  if (m_extension_offset >= m_extension_message.length())
    throw internal_error("PeerConnectionBase::up_extension bad offset.");

  uint32_t written = write_stream_throws(m_extension_message.data() + m_extension_offset, m_extension_message.length() - m_extension_offset);
  m_up->throttle()->node_used_unthrottled(written);
  m_extension_offset += written;

  if (m_extension_offset < m_extension_message.length())
    return false;

  m_extension_message.clear();

  // If we have an unprocessed message, process it now and enable reads again.
  if (m_extensions->is_complete() && !m_extensions->is_invalid()) {
    // DEBUG: What, this should fail when we block, no?
    if (!m_extensions->read_done())
      throw internal_error("PeerConnectionBase::up_extension could not process complete extension message.");

    this_thread::poll()->insert_read(this);
  }

  return true;
}

void
PeerConnectionBase::down_chunk_release() {
  if (m_down_chunk.is_valid())
    m_download->chunk_list()->release(&m_down_chunk, ChunkList::release_default);
}

void
PeerConnectionBase::up_chunk_release() {
  if (m_up_chunk.is_valid())
    m_download->chunk_list()->release(&m_up_chunk, ChunkList::release_default);
}

void
PeerConnectionBase::read_request_piece(const Piece& p) {
  auto upload_queue = m_peer_chunks.upload_queue();

  if (m_up_choke.choked() ||
      upload_queue->size() >= ProtocolExtension::max_request_queue_size ||
      p.length() > (1 << 17)) {
    LT_LOG_PIECE_EVENTS("(up)   request_ignored  %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
    return;
  }

  auto itr = std::find(upload_queue->begin(),
                       upload_queue->end(),
                       p);

  if (itr != upload_queue->end()) {
    LT_LOG_PIECE_EVENTS("(up)   request_ignored  %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
    return;
  }

  upload_queue->push_back(p);
  write_insert_poll_safe();

  LT_LOG_PIECE_EVENTS("(up)   request_added    %" PRIu32 " %" PRIu32 " %" PRIu32,
                      p.index(), p.offset(), p.length());
}

void
PeerConnectionBase::read_cancel_piece(const Piece& p) {
  auto itr = std::find(m_peer_chunks.upload_queue()->begin(),
                       m_peer_chunks.upload_queue()->end(),
                       p);

  if (itr != m_peer_chunks.upload_queue()->end()) {
    m_peer_chunks.upload_queue()->erase(itr);

    LT_LOG_PIECE_EVENTS("(up)   cancel_requested %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
  } else {
    LT_LOG_PIECE_EVENTS("(up)   cancel_ignored   %" PRIu32 " %" PRIu32 " %" PRIu32,
                        p.index(), p.offset(), p.length());
  }
}

void
PeerConnectionBase::write_prepare_piece() {
  m_up_piece = m_peer_chunks.upload_queue()->front();
  m_peer_chunks.upload_queue()->pop_front();

  // Move these checks somewhere else?
  if (!m_download->file_list()->is_valid_piece(m_up_piece) ||
      !m_download->file_list()->bitfield()->get(m_up_piece.index())) {
    char buffer[128];

    snprintf(buffer, 128, "Peer requested an invalid piece: %u %u %u",
             m_up_piece.index(), m_up_piece.length(), m_up_piece.offset());

    LT_LOG_PIECE_EVENTS("(up)   invalid_piece_in_upload_queue %" PRIu32 " %" PRIu32 " %" PRIu32,
                        m_up_piece.index(), m_up_piece.length(), m_up_piece.offset());

    throw communication_error(buffer);
  }

  m_up->write_piece(m_up_piece);

  LT_LOG_PIECE_EVENTS("(up)   prepared         %" PRIu32 " %" PRIu32 " %" PRIu32,
                      m_up_piece.index(), m_up_piece.length(), m_up_piece.offset());
}

void
PeerConnectionBase::write_prepare_extension(int type, const DataBuffer& message) {
  m_up->write_extension(m_extensions->id(type), message.length());

  m_extension_offset = 0;
  m_extension_message = message;

  // Need to encrypt the buffer, but not until the m_up
  // write buffer has been flushed, so flag it for now.
  if (is_encrypted())
    m_extension_offset = extension_must_encrypt;
}

// High stall count peers should request if we're *not* in endgame, or
// if we're in endgame and the download is too slow. Prefere not to request
// from high stall counts when we are doing decent speeds.
bool
PeerConnectionBase::should_request() {
  if (m_down_choke.choked() || !m_down_interested || !m_down_unchoked)
    // || m_down->get_state() == ProtocolRead::READ_SKIP_PIECE)
    return false;

  else if (!m_download->delegator()->get_aggressive())
    return true;

  else
    // We check if the peer is stalled, if it is not then we should
    // request. If the peer is stalled then we only request if the
    // download rate is below a certain value.
    return m_down_stall <= 1 || m_download->info()->down_rate()->rate() < (10 << 10);
}

bool
PeerConnectionBase::try_request_pieces() {
  if (request_list()->queued_empty())
    m_down_stall = 0;

  uint32_t pipeSize = request_list()->calculate_pipe_size(m_peer_chunks.download_throttle()->rate()->rate());

  // Don't start requesting if we can't do it in large enough chunks.
  if (request_list()->pipe_size() >= (pipeSize + 10) / 2)
    return false;

  bool success = false;

  while (request_list()->queued_size() < pipeSize && m_up->can_write_request()) {

    // It should get the right number the first time around, but loop just to be sure
    int maxRequests = m_up->max_write_request();
    int maxQueued = pipeSize - request_list()->queued_size();
    int maxPieces = std::max(std::min(maxRequests, maxQueued), 1);

    std::vector<const Piece*> pieces = request_list()->delegate(maxPieces);
    if (pieces.empty()) {
      return false;
    }

    for (auto& p : pieces) {
      if (!m_download->file_list()->is_valid_piece(*p) || !m_peer_chunks.bitfield()->get(p->index()))
        throw internal_error("PeerConnectionBase::try_request_pieces() tried to use an invalid piece.");

      m_up->write_request(*p);
      LT_LOG_PIECE_EVENTS("(down) requesting %" PRIu32 " %" PRIu32 " %" PRIu32,
                          p->index(), p->offset(), p->length());
      success = true;
    }
  }

  return success;
}

// Send one peer exchange message according to bits set in m_send_pex_mask.
// We can only send one message at a time, because the derived class
// needs to flush the buffer and call up_extension before the next one.
bool
PeerConnectionBase::send_pex_message() {
  if (!m_extensions->is_remote_supported(ProtocolExtension::UT_PEX)) {
    m_send_pex_mask = 0;
    return false;
  }

  // Message to tell peer to stop/start doing PEX is small so send it first.
  if (m_send_pex_mask & (PEX_ENABLE | PEX_DISABLE)) {
    if (!m_extensions->is_remote_supported(ProtocolExtension::UT_PEX))
      throw internal_error("PeerConnectionBase::send_pex_message() Not supported by peer.");

    write_prepare_extension(ProtocolExtension::HANDSHAKE,
                            ProtocolExtension::generate_toggle_message(ProtocolExtension::UT_PEX, (m_send_pex_mask & PEX_ENABLE) != 0));

    m_send_pex_mask &= ~(PEX_ENABLE | PEX_DISABLE);

  } else if (m_send_pex_mask & PEX_DO && m_extensions->id(ProtocolExtension::UT_PEX)) {
    const DataBuffer& pexMessage = m_download->get_ut_pex(m_extensions->is_initial_pex());
    m_extensions->clear_initial_pex();

    m_send_pex_mask &= ~PEX_DO;

    if (pexMessage.empty())
      return false;

    write_prepare_extension(ProtocolExtension::UT_PEX, pexMessage);

  } else {
    m_send_pex_mask = 0;
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
PeerConnectionBase::receive_metadata_piece([[maybe_unused]] uint32_t piece, [[maybe_unused]] const char* data, [[maybe_unused]] uint32_t length) {
}

} // namespace torrent
