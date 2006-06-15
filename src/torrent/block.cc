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

#include <algorithm>
#include <functional>
#include <rak/functional.h>

#include "block.h"
#include "block_list.h"
#include "block_transfer.h"
#include "exceptions.h"

namespace torrent {

inline void
Block::invalidate_transfer(BlockTransfer* transfer) {
  if (transfer == m_leader)
    throw internal_error("Block::invalidate_transfer(...) transfer == m_leader.");

  transfer->set_block(NULL);

  // FIXME: Various other accounting like position and counters.
  if (transfer->is_erased()) {
    delete transfer;
  } else {
    m_notStalled -= transfer->stall() == 0;

    // Not strictly needed.
    transfer->set_stall(BlockTransfer::stall_erased);
  }
}

void
Block::clear() {
  m_leader = NULL;
  m_position = 0;

  std::for_each(m_queued.begin(), m_queued.end(), std::bind1st(std::mem_fun(&Block::invalidate_transfer), this));
  m_queued.clear();

  std::for_each(m_transfers.begin(), m_transfers.end(), std::bind1st(std::mem_fun(&Block::invalidate_transfer), this));
  m_transfers.clear();

  if (m_notStalled != 0)
    throw internal_error("Block::clear() m_stalled != 0.");
}

BlockTransfer*
Block::insert(PeerInfo* peerInfo) {
  if (find_queued(peerInfo) || find_transfer(peerInfo))
    throw internal_error("Block::insert(...) find_queued(peerInfo) || find_transfer(peerInfo).");

  m_notStalled++;

  transfer_list::iterator itr = m_queued.insert(m_queued.end(), new BlockTransfer());

  (*itr)->set_peer_info(peerInfo);
  (*itr)->set_block(this);
  (*itr)->set_piece(m_piece);
  (*itr)->set_position(BlockTransfer::position_invalid);
  (*itr)->set_stall(0);

  return (*itr);
}
  
void
Block::erase(BlockTransfer* transfer) {
  if (transfer->is_erased())
    throw internal_error("Block::erase(...) transfer already erased.");

  m_notStalled -= transfer->stall() == 0;

  if (transfer->is_queued()) {
    transfer_list::iterator itr = std::find(m_queued.begin(), m_queued.end(), transfer);

    if (itr == m_queued.end())
      throw internal_error("Block::erase(...) Could not find transfer.");

    m_queued.erase(itr);

  } else {
    transfer_list::iterator itr = std::find(m_transfers.begin(), m_transfers.end(), transfer);

    if (itr == m_transfers.end())
      throw internal_error("Block::erase(...) Could not find transfer.");

    if (transfer == m_leader) {
      // This needs to be expanded, perhaps change the leader. But it
      // should work by just clearing it as the new leader takes over
      // when it passes position. 
      m_leader = NULL;
    }

    // Need to do something different here for now, i think.
    m_transfers.erase(itr);

//     transfer->set_stall(BlockTransfer::stall_erased);
  }

  delete transfer;
}

bool
Block::transfering(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("Block::transfering(...) transfer->block() == NULL.");

  transfer_list::iterator itr = std::find(m_queued.begin(), m_queued.end(), transfer);

  if (itr == m_queued.end())
    throw internal_error("Block::transfering(...) not queued.");

  transfer->set_position(0);

  m_queued.erase(itr);
  m_transfers.insert(m_transfers.end(), transfer);

  // If this block already has an active transfer, make this transfer
  // skip the piece. If this transfer gets ahead of the currently
  // transfering, it will (a) take over as the leader if the data is
  // the same or (b) erase itself from this block if the data does not
  // match.
  if (m_leader != NULL) {
    m_notStalled -= transfer->stall() == 0;
    transfer->set_stall(BlockTransfer::stall_not_leader);

    return false;
  }

  m_leader = transfer;
  return true;
}

void
Block::stalled_transfer(BlockTransfer* transfer) {
  if (transfer->stall() == BlockTransfer::stall_not_leader)
    return;

  if (transfer->stall() == 0) {
    if (m_notStalled == 0)
      throw internal_error("Block::stalled(...) m_notStalled == 0.");

    m_notStalled--;

    // Do magic here.
  }

  transfer->set_stall(transfer->stall() + 1);
}

bool
Block::completed(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("Block::completed(...) transfer->block() == NULL.");

  if (transfer->is_erased())
    throw internal_error("Block::completed(...) transfer is erased.");

  if (!transfer->is_leader())
    throw internal_error("Block::completed(...) transfer is not the leader.");

  if (transfer->is_queued())
    throw internal_error("Block::completed(...) transfer is queued.");

  // How does this check work now?
//   if (transfer->block()->is_finished())
//     throw internal_error("Block::completed(...) transfer is already marked as finished.");

  // Special case where another ignored transfer finished before the
  // leader?
  //
  // Perhaps do magic to the transfer, erase it or something.
  if (!is_finished()) {
    throw internal_error("Block::completed(...) !is_finished().");
  }

  m_parent->inc_finished();
  m_notStalled -= transfer->stall() == 0;

  transfer->set_stall(BlockTransfer::stall_erased);

  // Currently just throw out the queued transfers. In case the hash
  // check fails, we might consider telling pcb during the call to
  // Block::transfering(...). But that would propably not be correct
  // as we want to trigger cancel messages from here, as hash fail is
  // a rare occurrence.
  std::for_each(m_queued.begin(), m_queued.end(), std::bind1st(std::mem_fun(&Block::invalidate_transfer), this));
  m_queued.clear();

  // We need to invalidate those unfinished and keep the one that
  // finished for later reference.

  // Do a stable partition, accure the transfers that have completed
  // at the start. Use a 'try' counter that also adjust the sizes.

  // Temporary hack.
//   for (transfer_list::iterator itr = m_transfers.begin(), last = m_transfers.end(); itr != last; ) {
//     if ((*itr)->is_erased()) {
//       itr++;
//     } else {
//       itr = m_transfers.erase(itr);
//     }
//   }

  // Remove transfers that did not start (finish?).

  m_leader = NULL;

  std::for_each(m_transfers.begin(), m_transfers.end(), std::bind1st(std::mem_fun(&Block::invalidate_transfer), this));
  m_transfers.clear();

  return m_parent->is_all_finished();
}

BlockTransfer*
Block::find_queued(const PeerInfo* p) {
  transfer_list::iterator itr = std::find_if(m_queued.begin(), m_queued.end(), rak::equal(p, std::mem_fun(&BlockTransfer::peer_info)));

  if (itr == m_queued.end())
    return NULL;
  else
    return *itr;
}

const BlockTransfer*
Block::find_queued(const PeerInfo* p) const {
  transfer_list::const_iterator itr = std::find_if(m_queued.begin(), m_queued.end(), rak::equal(p, std::mem_fun(&BlockTransfer::peer_info)));

  if (itr == m_queued.end())
    return NULL;
  else
    return *itr;
}

BlockTransfer*
Block::find_transfer(const PeerInfo* p) {
  transfer_list::iterator itr = std::find_if(m_transfers.begin(), m_transfers.end(), rak::equal(p, std::mem_fun(&BlockTransfer::peer_info)));

  if (itr == m_transfers.end())
    return NULL;
  else
    return *itr;
}

const BlockTransfer*
Block::find_transfer(const PeerInfo* p) const {
  transfer_list::const_iterator itr = std::find_if(m_transfers.begin(), m_transfers.end(), rak::equal(p, std::mem_fun(&BlockTransfer::peer_info)));

  if (itr == m_transfers.end())
    return NULL;
  else
    return *itr;
}

}
