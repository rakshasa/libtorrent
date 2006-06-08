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

#include <functional>
#include <inttypes.h>
#include <rak/functional.h>

#include "torrent/block.h"
#include "torrent/block_list.h"
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
      m_piece.index() == d->block()->piece().index() &&
      m_piece.offset() == d->block()->piece().offset();
  }

  Piece m_piece;
};

const Piece*
RequestList::delegate() {
  BlockTransfer* r = m_delegator->delegate(m_peerChunks, m_affinity);

  if (r) {
    m_affinity = r->block()->index();
    m_queued.push_back(r);

    return &r->block()->piece();

  } else {
    return NULL;
  }
}

// Replace m_canceled with m_queued and set them to stalled.
void
RequestList::cancel() {
  std::for_each(m_canceled.begin(), m_canceled.end(), std::mem_fun(&BlockTransfer::release));
  m_canceled.clear();

  std::for_each(m_queued.begin(), m_queued.end(), std::mem_fun(&BlockTransfer::stalled));

  m_canceled.swap(m_queued);
}

void
RequestList::stall() {
  std::for_each(m_queued.begin(), m_queued.end(), std::mem_fun(&BlockTransfer::stalled));
}

void
RequestList::clear() {
  if (is_downloading())
    skipped();

  std::for_each(m_queued.begin(), m_queued.end(), std::mem_fun(&BlockTransfer::release));
  m_queued.clear();

  std::for_each(m_canceled.begin(), m_canceled.end(), std::mem_fun(&BlockTransfer::release));
  m_canceled.clear();
}

bool
RequestList::downloading(const Piece& piece) {
  if (m_transfer != NULL)
    throw internal_error("RequestList::downloading(...) m_transfer != NULL.");

  // Consider doing this on cancel_range.
  remove_invalid();

  ReserveeList::iterator itr = std::find_if(m_queued.begin(), m_queued.end(), request_list_same_piece(piece));

  if (itr == m_queued.end()) {
    itr = std::find_if(m_canceled.begin(), m_canceled.end(), request_list_same_piece(piece));

    if (itr == m_canceled.end()) {
      // Create a dummy BlockTransfer object to hold the piece
      // information.
      m_transfer = new BlockTransfer();
      m_transfer->set_peer_info(m_peerChunks->peer_info());
      m_transfer->set_block(NULL);
      m_transfer->set_piece(piece);
      m_transfer->set_position(0);
      m_transfer->set_stall(0);

      return false;
    }

    // Remove all up to and including itr.
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
    m_transfer->release();

    m_transfer = new BlockTransfer();
    m_transfer->set_peer_info(m_peerChunks->peer_info());
    m_transfer->set_block(NULL);
    m_transfer->set_piece(piece);
    m_transfer->set_position(0);
    m_transfer->set_stall(0);

    return false;
  }

  // This piece isn't wanted anymore. Do this after the length check
  // to ensure we return a correct BlockTransfer.
  if (!m_transfer->is_valid())
    return false;

  m_transfer->transfering();

  return true;
}

// Must clear the downloading piece.
void
RequestList::finished() {
  if (!is_downloading())
    throw internal_error("RequestList::finished() called but no transfer is in progress.");

  if (!m_transfer->is_valid())
    throw internal_error("RequestList::finished() called transfer is invalid.");

  BlockTransfer* transfer = m_transfer;
  m_transfer = NULL;

  m_delegator->finished(transfer);
}

void
RequestList::skipped() {
  if (!is_downloading())
    throw internal_error("RequestList::skip() called but no transfer is in progress.");

  m_transfer->release();
  m_transfer = NULL;
}

struct equals_reservee : public std::binary_function<BlockTransfer*, uint32_t, bool> {
  bool operator () (BlockTransfer* r, uint32_t index) const {
    return r->is_valid() && index == r->block()->index();
  }
};

bool
RequestList::is_interested_in_active() const {
  for (Delegator::Chunks::const_iterator itr = m_delegator->get_chunks().begin(), last = m_delegator->get_chunks().end(); itr != last; ++itr)
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
  while (m_queued.begin() != end) {
    m_queued.front()->stalled();
    
    m_canceled.push_back(m_queued.front());
    m_queued.pop_front();
  }
}

uint32_t
RequestList::remove_invalid() {
  uint32_t count = 0;
  ReserveeList::iterator itr;

  // Could be more efficient, but rarely do we find any.
  while ((itr = std::find_if(m_queued.begin(), m_queued.end(),  std::not1(std::mem_fun(&BlockTransfer::is_valid)))) != m_queued.end()) {
    count++;
    (*itr)->release();
    m_queued.erase(itr);
  }

  // Don't count m_canceled that are invalid.
  while ((itr = std::find_if(m_canceled.begin(), m_canceled.end(), std::not1(std::mem_fun(&BlockTransfer::is_valid)))) != m_canceled.end()) {
    (*itr)->release();
    m_canceled.erase(itr);
  }

  return count;
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
