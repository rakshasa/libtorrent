#include "config.h"

#include "request_list.h"

#include <algorithm>
#include <cassert>

#include "download/delegator.h"
#include "protocol/peer_chunks.h"
#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/exceptions.h"
#include "utils/instrumentation.h"

namespace torrent {

const instrumentation_enum request_list_constants::instrumentation_added[bucket_count] = {
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED,
  INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_ADDED,
  INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_ADDED,
  INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_ADDED
};
const instrumentation_enum request_list_constants::instrumentation_moved[bucket_count] = {
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_MOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_MOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_MOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_MOVED
};
const instrumentation_enum request_list_constants::instrumentation_removed[bucket_count] = {
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_REMOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_REMOVED,
  INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_REMOVED
};
const instrumentation_enum request_list_constants::instrumentation_total[bucket_count] = {
  INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL,
  INSTRUMENTATION_TRANSFER_REQUESTS_UNORDERED_TOTAL,
  INSTRUMENTATION_TRANSFER_REQUESTS_STALLED_TOTAL,
  INSTRUMENTATION_TRANSFER_REQUESTS_CHOKED_TOTAL
};

// Make inline...
template <>
void
request_list_constants::destroy<BlockTransfer*>(BlockTransfer*& obj) {
  Block::release(obj);
}

// TODO: Add a to-be-cancelled list, timer, and use that to avoid
// cancelling pieces on CHOKE->UNCHOKE weirdness in some clients.
//
// Right after this the client should always be allowed to queue more
// pieces, perhaps add a timer for last choke and check the request
// time on the queued pieces. This makes it possible to get going
// again if the remote queue got cleared.

// It is assumed invalid transfers have been removed.
struct request_list_same_piece {
  request_list_same_piece(const Piece& p) : m_piece(p) {}

  bool operator () (const BlockTransfer* d) const {
    return
      m_piece.index() == d->piece().index() &&
      m_piece.offset() == d->piece().offset();
  }

  Piece m_piece;
};

RequestList::~RequestList() {
  assert(m_transfer == nullptr);
  assert(m_queues.empty());

  torrent::this_thread::scheduler()->erase(&m_delay_remove_choked);
  torrent::this_thread::scheduler()->erase(&m_delay_process_unordered);
}

std::vector<const Piece*>
RequestList::delegate(uint32_t maxPieces) {
  std::vector<BlockTransfer*> transfers = m_delegator->delegate(m_peerChunks, m_affinity, maxPieces);

  std::vector<const Piece*> pieces;

  if (transfers.empty())
    return pieces;

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_DELEGATED, transfers.size());

  for (auto transfer : transfers) {
    m_queues.push_back(bucket_queued, transfer);
    pieces.push_back(&transfer->piece());
  }

  // Use the last index returned for the next affinity
  m_affinity = transfers.back()->index();

  return pieces;
}

void
RequestList::stall_initial() {
  queue_bucket_for_all_in_queue(m_queues, bucket_queued, &Block::stalled);
  m_queues.move_all_to(bucket_queued, bucket_stalled);
  queue_bucket_for_all_in_queue(m_queues, bucket_unordered, &Block::stalled);
  m_queues.move_all_to(bucket_unordered, bucket_stalled);
}

void
RequestList::stall_prolonged() {
  if (m_transfer != nullptr)
    Block::stalled(m_transfer);

  queue_bucket_for_all_in_queue(m_queues, bucket_queued, &Block::stalled);
  m_queues.move_all_to(bucket_queued, bucket_stalled);
  queue_bucket_for_all_in_queue(m_queues, bucket_unordered, &Block::stalled);
  m_queues.move_all_to(bucket_unordered, bucket_stalled);

  // Currently leave the the requests until the peer gets disconnected. (?)
}

void
RequestList::choked() {
  // Check if we want to update the choke timer; if non-zero and
  // updated within a short timespan?

  m_last_choke = torrent::this_thread::cached_time();

  if (m_queues.queue_empty(bucket_queued) && m_queues.queue_empty(bucket_unordered))
    return;

  m_queues.move_all_to(bucket_queued, bucket_choked);
  m_queues.move_all_to(bucket_unordered, bucket_choked);
  m_queues.move_all_to(bucket_stalled, bucket_choked);

  torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_delay_remove_choked, timeout_remove_choked);
}

void
RequestList::unchoked() {
  m_last_unchoke = torrent::this_thread::cached_time();

  // Clear choked queue if the peer doesn't start sending previously
  // requested pieces.
  //
  // This handles the case where a peer does a choke immediately
  // followed unchoke before starting to send pieces.

  if (!m_queues.queue_empty(bucket_choked))
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_delay_remove_choked, timeout_remove_choked);
  else
    torrent::this_thread::scheduler()->erase(&m_delay_remove_choked);
}

void
RequestList::delay_remove_choked() {
  m_queues.clear(bucket_choked);
}

void
RequestList::prepare_process_unordered(queues_type::iterator itr) {
  m_queues.move_to(bucket_queued, m_queues.begin(bucket_queued), itr,
                   bucket_unordered);

  if (m_delay_process_unordered.is_scheduled())
    return;

  torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_process_unordered, timeout_process_unordered);

  // TODO: Review if this should always be updated.
  m_last_unordered_position = unordered_size();
}

void
RequestList::delay_process_unordered() {
  m_last_unordered_position = std::min(m_last_unordered_position, unordered_size());

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED, m_last_unordered_position);

  m_queues.destroy(bucket_unordered,
                   m_queues.begin(bucket_unordered),
                   m_queues.begin(bucket_unordered) + m_last_unordered_position);

  m_last_unordered_position = unordered_size();

  if (m_last_unordered_position != 0)
    torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_process_unordered, timeout_process_unordered);
}

void
RequestList::clear() {
  if (is_downloading())
    skipped();

  m_queues.clear(bucket_queued);
  m_queues.clear(bucket_unordered);
  m_queues.clear(bucket_stalled);
  m_queues.clear(bucket_choked);
}

bool
RequestList::downloading(const Piece& piece) {
  if (m_transfer != nullptr)
    throw internal_error("RequestList::downloading(...) m_transfer != nullptr.");

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING, 1);

  std::pair<int, queues_type::iterator> itr =
    queue_bucket_find_if_in_any(m_queues, request_list_same_piece(piece));

  switch (itr.first) {
  case bucket_queued:
    if (itr.second != m_queues.begin(bucket_queued))
      prepare_process_unordered(itr.second);

    m_transfer = m_queues.take(itr.first, itr.second);
    break;

  case bucket_unordered:
    // Do unordered take of element here to avoid copy shifting the whole deque. (?)
    //
    // Alternatively, move back some elements to bucket_queued.

    if (std::distance(m_queues.begin(itr.first), itr.second) < static_cast<std::ptrdiff_t>(m_last_unordered_position))
      m_last_unordered_position--;

    m_transfer = m_queues.take(itr.first, itr.second);
    break;

  case bucket_stalled:
    // Do special handling of unordered pieces.

    m_transfer = m_queues.take(itr.first, itr.second);
    break;
  case bucket_choked:
    m_transfer = m_queues.take(itr.first, itr.second);

    // We make sure that the choked queue eventually gets cleared if
    // the peer has skipped sending some pieces from the choked queue.
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_delay_remove_choked, timeout_choked_received);
    break;

  default:
    goto downloading_error;
  }

  // We received an invalid piece length, propably zero length due to
  // the peer not being able to transfer the requested piece.
  //
  // We need to replace the current BlockTransfer so Block can keep
  // the unmodified BlockTransfer.
  if (piece.length() != m_transfer->piece().length()) {
    if (piece.length() != 0)
      throw communication_error("Peer sent a piece with wrong, non-zero, length.");

    Block::release(m_transfer);
    goto downloading_error;
  }

  // Check if piece isn't wanted anymore. Do this after the length
  // check to ensure we return a correct BlockTransfer.
  if (!m_transfer->is_valid())
    return false;

  m_transfer->block()->transfering(m_transfer);
  return true;

 downloading_error:
  // Create a dummy BlockTransfer object to hold the piece
  // information.
  m_transfer = new BlockTransfer();
  Block::create_dummy(m_transfer, m_peerChunks->peer_info(), piece);

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN, 1);

  return false;
}

// Must clear the downloading piece.
void
RequestList::finished() {
  if (!is_downloading())
    throw internal_error("RequestList::finished() called but no transfer is in progress.");

  if (!m_transfer->is_valid())
    throw internal_error("RequestList::finished() called but transfer is invalid.");

  BlockTransfer* transfer = m_transfer;
  m_transfer = nullptr;

  m_delegator->transfer_list()->finished(transfer);

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED, 1);
}

void
RequestList::skipped() {
  if (!is_downloading())
    throw internal_error("RequestList::skip() called but no transfer is in progress.");

  Block::release(m_transfer);
  m_transfer = nullptr;

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED, 1);
}

// Data downloaded by this non-leading transfer does not match what we
// already have.
void
RequestList::transfer_dissimilar() {
  if (!is_downloading())
    throw internal_error("RequestList::transfer_dissimilar() called but no transfer is in progress.");

  auto dummy = new BlockTransfer();

  Block::create_dummy(dummy, m_peerChunks->peer_info(), m_transfer->piece());
  dummy->set_position(m_transfer->position());

  // TODO.... peer_info still on a block we no longer control?..
  m_transfer->block()->transfer_dissimilar(m_transfer);
  m_transfer = dummy;
}

bool
RequestList::is_interested_in_active() const {
  auto list = m_delegator->transfer_list();
  return std::any_of(list->begin(), list->end(), [this](auto transfer) { return m_peerChunks->bitfield()->get(transfer->index()); });
}

uint32_t
RequestList::calculate_pipe_size(uint32_t rate) {
  // Change into KB.
  rate /= 1024;

  if (!m_delegator->get_aggressive()) {
    if (rate < 20)
      return rate + 2;
    else
      return rate / 5 + 18;

  } else {
    if (rate < 10)
      return rate / 5 + 1;
    else
      return rate / 10 + 2;
  }
}

} // namespace torrent
