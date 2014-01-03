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

// TODO: Add a to-be-cancelled list, timer, and use that to avoid
// cancelling pieces on CHOKE->UNCHOKE weirdness in some clients.
//
// Right after this the client should always be allowed to queue more
// pieces, perhaps add a timer for last choke and check the request
// time on the queued pieces. This makes it possible to get going
// again if the remote queue got cleared.
//
// Add unit tests...

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

inline BlockTransfer*
RequestList::pop_front_queued() {
  BlockTransfer* transfer = m_queued.front();
  m_queued.pop_front();

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED, 1);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL, -1);

  return transfer;
}

inline void
RequestList::push_back_queued(BlockTransfer* r) {
  m_queued.push_back(r);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_ADDED, 1);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL, 1);
}

inline void
RequestList::push_back_canceled(BlockTransfer* r) {
  m_canceled.push_back(r);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_ADDED, 1);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL, 1);
}

inline void
RequestList::release_queued_range(ReserveeList::iterator begin, ReserveeList::iterator end) {
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED, std::distance(begin, end));
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL, -(int64_t)std::distance(begin, end));

  std::for_each(begin, end, std::ptr_fun(&Block::release));
  m_queued.erase(begin, end);
}

inline void
RequestList::release_canceled_range(ReserveeList::iterator begin, ReserveeList::iterator end) {
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_REMOVED, std::distance(begin, end));
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL, -(int64_t)std::distance(begin, end));

  std::for_each(begin, end, std::ptr_fun(&Block::release));
  m_canceled.erase(begin, end);
}

inline void
RequestList::move_to_canceled_range(ReserveeList::iterator begin, ReserveeList::iterator end) {
  int64_t range_size = std::distance(begin, end);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_ADDED, range_size);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL, range_size);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_REMOVED, range_size);
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL,  -range_size);

  std::for_each(begin, end, std::ptr_fun(&Block::stalled));

  if (m_canceled.empty() && begin == m_queued.begin() && end == m_queued.end()) {
    m_canceled.swap(m_queued);
  } else {
    m_canceled.insert(m_canceled.end(), begin, end);
    m_queued.erase(begin, end);
  }
}

inline void
RequestList::move_queued_to_transferring(ReserveeList::iterator itr) {
  m_transfer = *itr;
  m_queued.erase(itr);

  // When we start a normal transfer we don't count it as removed.
  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_QUEUED_TOTAL, -1);
}

inline void
RequestList::move_canceled_to_transferring(ReserveeList::iterator itr) {
  m_transfer = *itr;
  m_canceled.erase(itr);

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_CANCELED_TOTAL, -1);
}

const Piece*
RequestList::delegate() {
  BlockTransfer* r = m_delegator->delegate(m_peerChunks, m_affinity);

  if (r) {
    m_affinity = r->index();
    push_back_queued(r);

    return &r->piece();

  } else {
    return NULL;
  }
}

// Replace m_canceled with m_queued and set them to stalled.
//
// This doesn't seem entirely correct... Perhaps canceled requests
// should be kept around until we hit a safe state where we may throw
// them out?
void
RequestList::cancel() {
  release_canceled_range(m_canceled.begin(), m_canceled.end());
  move_to_canceled_range(m_queued.begin(), m_queued.end());
}

void
RequestList::stall() {
  if (m_transfer != NULL)
    Block::stalled(m_transfer);

  std::for_each(m_queued.begin(), m_queued.end(), std::ptr_fun(&Block::stalled));
}

void
RequestList::clear() {
  if (is_downloading())
    skipped();

  release_queued_range(m_queued.begin(), m_queued.end());
  release_canceled_range(m_canceled.begin(), m_canceled.end());
}

bool
RequestList::downloading(const Piece& piece) {
  if (m_transfer != NULL)
    throw internal_error("RequestList::downloading(...) m_transfer != NULL.");

  ReserveeList::iterator itr = std::find_if(m_queued.begin(), m_queued.end(), request_list_same_piece(piece));

  if (itr == m_queued.end()) {
    itr = std::find_if(m_canceled.begin(), m_canceled.end(), request_list_same_piece(piece));

    if (itr == m_canceled.end()) {
      // Consider counting these pieces as spam.
      instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN, 1);
      goto downloading_error;
    }

    move_canceled_to_transferring(itr);

  } else if (itr == m_queued.end()) {
    move_queued_to_transferring(itr);

  } else {
    cancel_range(itr);
    move_queued_to_transferring(itr);
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

    instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_UNKNOWN, 1);
    goto downloading_error;
  }

  instrumentation_update(INSTRUMENTATION_TRANSFER_REQUESTS_DOWNLOADING, 1);

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

bool
RequestList::has_index(uint32_t index) {
  return std::find_if(m_queued.begin(), m_queued.end(), std::bind2nd(equals_reservee(), index)) != m_queued.end();
}

struct request_list_keep_request {
  bool operator () (const BlockTransfer* d) {
    return d->is_valid();
  }
};

void
RequestList::cancel_range(ReserveeList::iterator end) {
  // This only gets called when it's downloading a non-canceled piece,
  // so to avoid a backlog of canceled pieces we need to empty it
  // here.
  //
  // This may cause us to skip pieces if the peer does some strange
  // reordering.
  //
  // Add some extra checks here to avoid clearing too often.
  if (!m_canceled.empty()) {
    // Old buggy...
    // release_canceled_range(m_canceled.begin(), m_canceled.end());


    // Only release if !valid or... if we've been choked/unchoked
    // since last time, include a timer for both choke and unchoke.

    // First remove all the !valid pieces...
    ReserveeList::iterator itr = std::partition(m_canceled.begin(), m_canceled.end(), request_list_keep_request());

    release_canceled_range(itr, m_canceled.end());
  }

  while (m_queued.begin() != end) {
    BlockTransfer* transfer = pop_front_queued();

    if (request_list_keep_request()(transfer)) {
      Block::stalled(transfer);
      push_back_canceled(transfer);

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
