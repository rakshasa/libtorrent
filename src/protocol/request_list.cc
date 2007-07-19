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

#include <algorithm>
#include <functional>
#include <inttypes.h>
#include <rak/functional.h>

#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/exceptions.h"
#include "download/delegator.h"

#include "peer_chunks.h"
#include "request_list.h"

namespace torrent {

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

const Piece*
RequestList::delegate() {
  BlockTransfer* r = m_delegator->delegate(m_peerChunks, m_affinity);

  if (r) {
    m_affinity = r->index();
    m_queued.push_back(r);

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
  std::for_each(m_canceled.begin(), m_canceled.end(), std::ptr_fun(&Block::release));
  m_canceled.clear();

  std::for_each(m_queued.begin(), m_queued.end(), std::ptr_fun(&Block::stalled));

  m_canceled.swap(m_queued);
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

  std::for_each(m_queued.begin(), m_queued.end(), std::ptr_fun(&Block::release));
  m_queued.clear();

  std::for_each(m_canceled.begin(), m_canceled.end(), std::ptr_fun(&Block::release));
  m_canceled.clear();
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
      goto downloading_error;
    }

    m_transfer = *itr;
    m_canceled.erase(itr);

  } else {
    m_transfer = *itr;
    cancel_range(itr);
    m_queued.pop_front();
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
}

void
RequestList::skipped() {
  if (!is_downloading())
    throw internal_error("RequestList::skip() called but no transfer is in progress.");

  Block::release(m_transfer);
  m_transfer = NULL;
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

// void
// RequestList::cancel_transfer(BlockTransfer* transfer) {
// //   ReserveeList itr = std::find_if(m_canceled.begin(), m_canceled.end(), request_list_same_piece(piece));

// //   ReserveeList itr = std::find_if(m_canceled.begin(), m_canceled.end(), request_list_same_piece(piece));
// }

struct equals_reservee : public std::binary_function<BlockTransfer*, uint32_t, bool> {
  bool operator () (BlockTransfer* r, uint32_t index) const {
    return r->is_valid() && index == r->index();
  }
};

bool
RequestList::is_interested_in_active() const {
  for (TransferList::const_iterator itr = m_delegator->transfer_list()->begin(), last = m_delegator->transfer_list()->end(); itr != last; ++itr)
    if (m_peerChunks->bitfield()->get((*itr)->index()))
      return true;

  return false;
}

bool
RequestList::has_index(uint32_t index) {
  return std::find_if(m_queued.begin(), m_queued.end(), std::bind2nd(equals_reservee(), index)) != m_queued.end();
}

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
    std::for_each(m_canceled.begin(), m_canceled.end(), std::ptr_fun(&Block::release));
    m_canceled.clear();
  }

  while (m_queued.begin() != end) {
    BlockTransfer* transfer = m_queued.front();
    m_queued.pop_front();

    if (transfer->is_valid()) {
      Block::stalled(transfer);
      m_canceled.push_back(transfer);
    } else {
      Block::release(transfer);
    }
  }
}

uint32_t
RequestList::calculate_pipe_size(uint32_t rate) {
  // Change into KB.
  rate /= 1024;

  // Request enough to have enough for 16 seconds at fast rates.

  if (!m_delegator->get_aggressive()) {
    uint32_t queueKB = std::min(rate * 30, (uint32_t)16 << 10);
  
    return (queueKB / (Delegator::block_size / 1024)) + 2;

  } else {
    uint32_t queueKB = std::min(rate * 16, (uint32_t)16 << 10);
  
    return (queueKB / (Delegator::block_size / 1024)) + 1;
  }
}

}
