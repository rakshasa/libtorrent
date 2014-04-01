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

#include <algorithm>
#include <functional>
#include <inttypes.h>
#include <rak/functional.h>

#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/exceptions.h"
#include "download/delegator.h"
#include "utils/instrumentation.h"

#include "peer_chunks.h"
#include "request_list.h"

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

  bool operator () (const BlockTransfer* d) {
    return
      m_piece.index() == d->piece().index() &&
      m_piece.offset() == d->piece().offset();
  }

  Piece m_piece;
};

RequestList::~RequestList() {
  if (m_transfer != NULL)
    throw internal_error("request dtor m_transfer != NULL");

  if (!m_queues.empty())
    throw internal_error("request dtor m_queues not empty");

  priority_queue_erase(&taskScheduler, &m_delay_remove_choked);
  priority_queue_erase(&taskScheduler, &m_delay_process_unordered);
}

const Piece*
RequestList::delegate() {
  BlockTransfer* transfer = m_delegator->delegate(m_peerChunks, m_affinity);

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_DELEGATED, 1);

  if (transfer == NULL)
    return NULL;

  m_affinity = transfer->index();
  m_queues.push_back(bucket_queued, transfer);

  return &transfer->piece();
}

void
RequestList::stall_initial() {
  m_queues.clear(bucket_unordered);

  queue_bucket_for_all_in_queue(m_queues, bucket_unordered, std::ptr_fun(&Block::stalled));
  m_queues.move_all_to(bucket_queued, bucket_unordered);
}

void
RequestList::stall_prolonged() {
  if (m_transfer != NULL)
    Block::stalled(m_transfer);

  queue_bucket_for_all_in_queue(m_queues, bucket_queued, std::ptr_fun(&Block::stalled));
}

void
RequestList::choked() {
  // Check if we want to update the choke timer; if non-zero and
  // updated within a short timespan?

  m_last_choke = cachedTime;

  if (m_queues.queue_empty(bucket_queued) && m_queues.queue_empty(bucket_unordered))
    return;

  m_queues.move_all_to(bucket_queued, bucket_choked);
  m_queues.move_all_to(bucket_unordered, bucket_choked);
  m_queues.move_all_to(bucket_stalled, bucket_choked);

  if (!m_delay_remove_choked.is_queued())
    priority_queue_insert(&taskScheduler, &m_delay_remove_choked,
                          (cachedTime + rak::timer::from_seconds(6)).round_seconds());
}

void
RequestList::unchoked() {
  m_last_unchoke = cachedTime;

  priority_queue_erase(&taskScheduler, &m_delay_remove_choked);

  // Clear choked queue if the peer doesn't start sending previously
  // requested pieces.
  //
  // This handles the case where a peer does a choke immediately
  // followed unchoke before starting to send pieces.
  if (!m_queues.queue_empty(bucket_choked)) {
    priority_queue_insert(&taskScheduler, &m_delay_remove_choked,
                          (cachedTime + rak::timer::from_seconds(60)).round_seconds());
  }
}

void
RequestList::delay_remove_choked() {
  m_queues.clear(bucket_choked);
}

void
RequestList::delay_process_unordered() {

  // Remove requests 
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
  if (m_transfer != NULL)
    throw internal_error("RequestList::downloading(...) m_transfer != NULL.");

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING, 1);

  std::pair<int, queues_type::iterator> itr =
    queue_bucket_find_if_in_any(m_queues, request_list_same_piece(piece));

  switch (itr.first) {
  case bucket_queued:
    if (itr.second != m_queues.begin(bucket_queued))
      cancel_range(itr.second);

    m_transfer = m_queues.take(itr.first, itr.second);
    break;
  case bucket_unordered:
    m_transfer = m_queues.take(itr.first, itr.second);
    break;
  case bucket_stalled:
    m_transfer = m_queues.take(itr.first, itr.second);
    break;
  case bucket_choked:
    m_transfer = m_queues.take(itr.first, itr.second);

    // We make sure that the choked queue eventually gets cleared if
    // the peer has skipped sending some pieces from the choked queue.
    priority_queue_erase(&taskScheduler, &m_delay_remove_choked);
    priority_queue_insert(&taskScheduler, &m_delay_remove_choked,
                          (cachedTime + rak::timer::from_seconds(60)).round_seconds());
    break;
  default:
    goto downloading_error;
  };

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
  m_transfer = NULL;

  m_delegator->transfer_list()->finished(transfer);

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_FINISHED, 1);
}

void
RequestList::skipped() {
  if (!is_downloading())
    throw internal_error("RequestList::skip() called but no transfer is in progress.");

  Block::release(m_transfer);
  m_transfer = NULL;

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_SKIPPED, 1);
}

// Data downloaded by this non-leading transfer does not match what we
// already have.
void
RequestList::transfer_dissimilar() {
  if (!is_downloading())
    throw internal_error("RequestList::transfer_dissimilar() called but no transfer is in progress.");

  BlockTransfer* dummy = new BlockTransfer();
  Block::create_dummy(dummy, m_peerChunks->peer_info(), m_transfer->piece());
  dummy->set_position(m_transfer->position());

  // TODO.... peer_info still on a block we no longer control?..
  m_transfer->block()->transfer_dissimilar(m_transfer);
  m_transfer = dummy;
}

struct equals_reservee : public std::binary_function<BlockTransfer*, uint32_t, bool> {
  bool operator () (BlockTransfer* r, uint32_t index) const {
    return r->is_valid() && index == r->index();
  }
};

bool
RequestList::is_interested_in_active() const {
  for (TransferList::const_iterator
         itr = m_delegator->transfer_list()->begin(),
         last = m_delegator->transfer_list()->end(); itr != last; ++itr)
    if (m_peerChunks->bitfield()->get((*itr)->index()))
      return true;

  return false;
}

struct request_list_keep_request {
  bool operator () (const BlockTransfer* d) {
    return d->is_valid();
  }
};

void
RequestList::cancel_range(queues_type::iterator end) {
  // This only gets called when it's downloading a non-unordered piece,
  // so to avoid a backlog of unordered pieces we need to empty it
  // here.
  //
  // This may cause us to skip pieces if the peer does some strange
  // reordering.
  //
  // Add some extra checks here to avoid clearing too often.
  if (!m_queues.queue_empty(bucket_unordered)) {
    // Old buggy...
    // release_unordered_range(m_unordered.begin(), m_queues.end(bucket_unordered));


    // Only release if !valid or... if we've been choked/unchoked
    // since last time, include a timer for both choke and unchoke.

    // First remove all the !valid pieces...
    queues_type::iterator itr = std::partition(m_queues.begin(bucket_unordered), m_queues.end(bucket_unordered),
                                                request_list_keep_request());

    m_queues.destroy(bucket_unordered, itr, m_queues.end(bucket_unordered));
  }

  while (m_queues.begin(bucket_queued) != end) {
    BlockTransfer* transfer = m_queues.pop_and_front(bucket_queued);

    if (request_list_keep_request()(transfer)) {
      Block::stalled(transfer);
      m_queues.push_back(bucket_unordered, transfer);

    } else {
      Block::release(transfer);
    }
  }
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

}
