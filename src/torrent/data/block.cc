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

#include "peer/peer_info.h"
#include "protocol/peer_connection_base.h"

#include "block.h"
#include "block_failed.h"
#include "block_list.h"
#include "block_transfer.h"
#include "exceptions.h"

namespace torrent {

Block::~Block() {
  if (m_state != STATE_INCOMPLETE && m_state != STATE_COMPLETED)
    throw internal_error("Block dtor with 'm_state != STATE_INCOMPLETE && m_state != STATE_COMPLETED'");

  if (m_state == STATE_COMPLETED) {
    if (m_leader == NULL)
      throw internal_error("Block dtor with 'm_state == STATE_COMPLETED && m_leader == NULL'");

    m_leader->set_peer_info(NULL);
  }

  m_leader = NULL;
  m_state = STATE_INVALID;

  for (auto& block : m_queued) {
    invalidate_transfer(block);
  }
  m_queued.clear();

  for (auto& block : m_transfers) {
    invalidate_transfer(block);
  }
  m_transfers.clear();

  if (m_notStalled != 0)
    throw internal_error("Block::clear() m_stalled != 0.");

  delete m_failedList;
}

bool
Block::is_finished() const {
  return m_leader != nullptr && m_leader->is_finished();
}

bool
Block::is_transfering() const {
  return m_leader != nullptr && !m_leader->is_finished();
}

BlockTransfer*
Block::insert(PeerInfo* peerInfo) {
  if (find_queued(peerInfo) || find_transfer(peerInfo))
    return NULL;

  m_notStalled++;

  auto itr = m_queued.insert(m_queued.end(), new BlockTransfer());

  (*itr)->set_peer_info(peerInfo);
  (*itr)->set_block(this);
  (*itr)->set_piece(m_piece);
  (*itr)->set_state(BlockTransfer::STATE_QUEUED);
  (*itr)->set_request_time(this_thread::cached_seconds().count());
  (*itr)->set_position(0);
  (*itr)->set_stall(0);
  (*itr)->set_failed_index(BlockFailed::invalid_index);

  return (*itr);
}
  
void
Block::erase(BlockTransfer* transfer) {
  if (transfer->is_erased())
    throw internal_error("Block::erase(...) transfer already erased");

  if (transfer->peer_info() != NULL)
    throw internal_error("Block::erase(...) transfer has non-null peer info");

  m_notStalled -= transfer->stall() == 0;

  if (transfer->is_queued()) {
    auto itr = std::find(m_queued.begin(), m_queued.end(), transfer);

    if (itr == m_queued.end())
      throw internal_error("Block::erase(...) Could not find transfer.");

    m_queued.erase(itr);

  } else if (!transfer->is_finished()) {
    auto itr = std::find(m_transfers.begin(), m_transfers.end(), transfer);

    if (itr == m_transfers.end())
      throw internal_error("Block::erase(...) Could not find transfer.");

    // Need to do something different here for now, i think.
    m_transfers.erase(itr);

    if (transfer == m_leader) {

      if (m_state == STATE_COMPLETED)
        throw internal_error("Block::erase with 'transfer == m_transfer && m_state == STATE_COMPLETED'");

      // When the leader is erased then any non-leading transfer must
      // be promoted. These non-leading transfers are guaranteed to
      // have the same data up to their position. PeerConnectionBase
      // assumes that a Block with non-leaders have a leader.

      // Create a range containing transfers with
      // is_not_leader(). Erased transfer will end up in the back.

      auto first = std::find_if_not(m_transfers.begin(), m_transfers.end(), std::mem_fn(&BlockTransfer::is_leader));
      auto last  = std::stable_partition(first, m_transfers.end(), std::mem_fn(&BlockTransfer::is_not_leader));

      auto new_leader = std::max_element(first, last, [](BlockTransfer* t1, BlockTransfer* t2) {
        return t1->position() < t2->position();
      });

      if (new_leader != last) {
        m_leader = *new_leader;
        m_leader->set_state(BlockTransfer::STATE_LEADER);
      } else {
        m_leader = NULL;

        // If we have no new leader, remove the erased (dissimilar)
        // transfers so they can get another shot. They cannot be
        // removed when found dissimilar as that would result in them
        // being queued immediately.
        remove_erased_transfers();
      }
    }

  } else {
    throw internal_error("Block::erase(...) Transfer is finished.");
  }

  // Check if we need to check for peer_info not being null.

  transfer->set_block(NULL);
  delete transfer;
}

bool
Block::transfering(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("Block::transfering(...) transfer->block() == NULL.");

  auto itr = std::find(m_queued.begin(), m_queued.end(), transfer);

  if (itr == m_queued.end())
    throw internal_error("Block::transfering(...) not queued.");

  m_queued.erase(itr);
  m_transfers.insert(m_transfers.end(), transfer);

  // If this block already has an active transfer, make this transfer
  // skip the piece. If this transfer gets ahead of the currently
  // transfering, it will (a) take over as the leader if the data is
  // the same or (b) erase itself from this block if the data does not
  // match.
  if (m_leader != NULL) {
    transfer->set_state(BlockTransfer::STATE_NOT_LEADER);
    return false;

  } else {
    m_leader = transfer;

    transfer->set_state(BlockTransfer::STATE_LEADER);
    return true;
  }
}

// TODO: Don't depend on m_leader for access to block transfer data of
// done peer_info, etc.
bool
Block::completed(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("Block::completed(...) transfer->block() == NULL.");

  if (!transfer->is_leader())
    throw internal_error("Block::completed(...) transfer is not the leader.");

  // Special case where another ignored transfer finished before the
  // leader?
  //
  // Perhaps do magic to the transfer, erase it or something.
  if (!is_finished())
    throw internal_error("Block::completed(...) !is_finished().");

  if (transfer != m_leader)
    throw internal_error("Block::completed(...) transfer != m_leader.");

  m_parent->inc_finished();

  if (static_cast<Block::size_type>(std::count_if(m_parent->begin(), m_parent->end(), std::mem_fn(&Block::is_finished))) < m_parent->finished())
    throw internal_error("Block::completed(...) Finished blocks too large.");

  m_notStalled -= transfer->stall() == 0;

  transfer->set_block(NULL);
  transfer->set_stall(~uint32_t());

  // Currently just throw out the queued transfers. In case the hash
  // check fails, we might consider telling pcb during the call to
  // Block::transfering(...). But that would propably not be correct
  // as we want to trigger cancel messages from here, as hash fail is
  // a rare occurrence.
  for (const auto& block : m_queued) {
    invalidate_transfer(block);
  }
  m_queued.clear();

  // We need to invalidate those unfinished and keep the one that
  // finished for later reference.
  remove_non_leader_transfers();
  
  // We now know that all transfers except the current leader we're
  // handling has been invalidated.
  if (m_transfers.empty() || m_transfers.back() != transfer)
    throw internal_error("Block::completed(...) m_transfers.empty() || m_transfers.back() != transfer.");

  m_state = STATE_COMPLETED;

  return m_parent->is_all_finished();
}

void
Block::retry_transfer() {
  m_state = STATE_INCOMPLETE;
}

// Mark a non-leading transfer as having received dissimilar data to
// the leader. It is then marked as erased so that we know its data
// was not used, yet keep it in m_transfers so as not to cause a
// re-download.
void
Block::transfer_dissimilar(BlockTransfer* transfer) {
  if (!transfer->is_not_leader() || m_leader == transfer)
    throw internal_error("Block::transfer_dissimilar(...) transfer is the leader.");

  m_notStalled -= transfer->stall() == 0;

  // Why not just delete? Gets done by completed(), though when
  // erasing the leader we need to remove dissimilar unless we have
  // another leader.
  
  transfer->set_state(BlockTransfer::STATE_ERASED);
  transfer->set_position(0);
  transfer->set_block(NULL);
}

void
Block::stalled(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    return;

  transfer->block()->stalled_transfer(transfer);
}

void
Block::stalled_transfer(BlockTransfer* transfer) {
  if (transfer->stall() == 0) {
    if (m_notStalled == 0)
      throw internal_error("Block::stalled(...) m_notStalled == 0.");

    m_notStalled--;

    // Do magic here.
  }

  transfer->set_stall(transfer->stall() + 1);
}

void
Block::change_leader(BlockTransfer* transfer) {
  if (m_leader == transfer)
    throw internal_error("Block::change_leader(...) m_leader == transfer.");

  if (is_finished())
    throw internal_error("Block::change_leader(...) is_finished().");

  if (m_leader != NULL)
    m_leader->set_state(BlockTransfer::STATE_NOT_LEADER);

  m_leader = transfer;
  m_leader->set_state(BlockTransfer::STATE_LEADER);
}

void
Block::failed_leader() {
  if (!is_finished())
    throw internal_error("Block::failed_leader(...) !is_finished().");

  m_leader = NULL;

  if (m_failedList != NULL)
    m_failedList->set_current(BlockFailed::invalid_index);
}

void
Block::create_dummy(BlockTransfer* transfer, PeerInfo* peerInfo, const Piece& piece) {
  transfer->set_peer_info(peerInfo);

  transfer->set_block(NULL);
  transfer->set_piece(piece);
  transfer->set_state(BlockTransfer::STATE_ERASED);

  transfer->set_position(0);
  transfer->set_stall(0);
  transfer->set_failed_index(BlockTransfer::invalid_index);
}

void
Block::release(BlockTransfer* transfer) {
  transfer->set_peer_info(NULL);

  if (!transfer->is_valid())
    delete transfer;
  else
    // TODO: Do we need to verify that the block is 'this'?
    transfer->block()->erase(transfer);
}

//
// Private:
//

void
Block::invalidate_transfer(BlockTransfer* transfer) {
  if (transfer == m_leader)
    throw internal_error("Block::invalidate_transfer(...) transfer == m_leader.");

  // Check if the block is this.

  transfer->set_block(NULL);

  if (transfer->peer_info() == NULL) {
    delete transfer;
    return; // Consider if this should be an exception.
  }

  m_notStalled -= (transfer->stall() == 0);

  // Do the canceling magic here. 
  if (transfer->peer_info()->connection() != NULL)
    transfer->peer_info()->connection()->cancel_transfer(transfer);
}

void
Block::remove_erased_transfers() {
  auto split = std::stable_partition(m_transfers.begin(), m_transfers.end(), [](auto block) { return !block->is_erased(); });

  std::for_each(split, m_transfers.end(), [this](auto block) { invalidate_transfer(block); });
  m_transfers.erase(split, m_transfers.end());
}

void
Block::remove_non_leader_transfers() {
  auto split = std::stable_partition(m_transfers.begin(), m_transfers.end(), std::mem_fn(&BlockTransfer::is_leader));

  std::for_each(split, m_transfers.end(), [this](auto block) { invalidate_transfer(block); });
  m_transfers.erase(split, m_transfers.end());
}

BlockTransfer*
Block::find_queued(const PeerInfo* p) {
  auto itr = std::find_if(m_queued.begin(), m_queued.end(), [p](BlockTransfer* t) { return p == t->peer_info(); });

  if (itr == m_queued.end())
    return NULL;
  else
    return *itr;
}

const BlockTransfer*
Block::find_queued(const PeerInfo* p) const {
  auto itr = std::find_if(m_queued.begin(), m_queued.end(), [p](BlockTransfer* t) { return p == t->peer_info(); });

  if (itr == m_queued.end())
    return NULL;
  else
    return *itr;
}

BlockTransfer*
Block::find_transfer(const PeerInfo* p) {
  auto itr = std::find_if(m_transfers.begin(), m_transfers.end(), [p](BlockTransfer* t) { return p == t->peer_info(); });

  if (itr == m_transfers.end())
    return NULL;
  else
    return *itr;
}

const BlockTransfer*
Block::find_transfer(const PeerInfo* p) const {
  auto itr = std::find_if(m_transfers.begin(), m_transfers.end(), [p](BlockTransfer* t) { return p == t->peer_info(); });

  if (itr == m_transfers.end())
    return NULL;
  else
    return *itr;
}

} // namespace torrent
