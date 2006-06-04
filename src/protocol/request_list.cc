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
    m_reservees.push_back(r);

    return &r->block()->piece();

  } else {
    return NULL;
  }
}

// Replace m_canceled with m_reservees and set them to stalled.
void
RequestList::cancel() {
  if (m_downloading)
    throw internal_error("RequestList::cancel(...) called while is_downloading() == true");

  std::for_each(m_canceled.begin(), m_canceled.end(), std::ptr_fun(&Block::erase));
  m_canceled.clear();

  std::for_each(m_reservees.begin(), m_reservees.end(), std::ptr_fun(&Block::stalled));

  m_canceled.swap(m_reservees);
}

void
RequestList::stall() {
  std::for_each(m_reservees.begin(), m_reservees.end(), std::ptr_fun(&Block::stalled));
}

bool
RequestList::downloading(const Piece& p) {
  if (m_downloading)
    throw internal_error("RequestList::downloading(...) bug, m_downloading is already set");

  remove_invalid();

  ReserveeList::iterator itr = std::find_if(m_reservees.begin(), m_reservees.end(), request_list_same_piece(p));
  
  if (itr == m_reservees.end()) {
    ReserveeList::iterator canceledItr = std::find_if(m_canceled.begin(), m_canceled.end(), request_list_same_piece(p));

    if (canceledItr == m_canceled.end())
      return false;

    itr = m_reservees.insert(m_reservees.begin(), *canceledItr);
    m_canceled.erase(canceledItr);

  } else {
    cancel_range(itr);
  }
  
  // Make sure pieces are removed from the reservee list if the peer
  // returns zero length pieces.
  if (p.length() != (*itr)->block()->piece().length()) {
    m_reservees.erase(itr);
    return false;
  }

  Block::transfering(*itr);
  m_downloading = true;

  if (p != m_reservees.front()->block()->piece())
    throw internal_error("RequestList::downloading(...) did not add the new piece to the front of the list");
  
  return true;
}

// Must clear the downloading piece.
void
RequestList::finished() {
  if (!m_downloading || !m_reservees.size())
    throw internal_error("RequestList::finished() called without a downloading piece");

  m_delegator->finished(m_reservees.front());
  m_reservees.pop_front();

  m_downloading = false;
}

void
RequestList::skip() {
  if (!m_downloading || !m_reservees.size())
    throw internal_error("RequestList::skip() called without a downloading piece");

  Block::erase(m_reservees.front());
  m_reservees.pop_front();

  m_downloading = false;
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
  return std::find_if(m_reservees.begin(), m_reservees.end(), std::bind2nd(equals_reservee(), index))
    != m_reservees.end();
}

void
RequestList::cancel_range(ReserveeList::iterator end) {
  while (m_reservees.begin() != end) {
    Block::stalled(m_reservees.front());
    
    m_canceled.push_back(m_reservees.front());
    m_reservees.pop_front();
  }
}

uint32_t
RequestList::remove_invalid() {
  uint32_t count = 0;
  ReserveeList::iterator itr;

  // Could be more efficient, but rarely do we find any.
  while ((itr = std::find_if(m_reservees.begin(), m_reservees.end(),  std::not1(std::mem_fun(&BlockTransfer::is_valid)))) != m_reservees.end()) {
    count++;
    delete *itr;
    m_reservees.erase(itr);
  }

  // Don't count m_canceled that are invalid.
  while ((itr = std::find_if(m_canceled.begin(), m_canceled.end(), std::not1(std::mem_fun(&BlockTransfer::is_valid)))) != m_canceled.end()) {
    delete *itr;
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


//   if ()
//     if (s < 50000)
//       return std::max((uint32_t)2, (s + 2000) / 2000);
//     else
//       return std::min((uint32_t)200, (s + 160000) / 4000);

//   else
//     if (s < 4000)
//       return 1;
//     else
//       return std::min((uint32_t)80, (s + 32000) / 8000);

// }

}
