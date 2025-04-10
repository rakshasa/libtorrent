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
#include <set>
#include <rak/timer.h>

#include "data/chunk.h"
#include "peer/peer_info.h"

#include "block_failed.h"
#include "block_transfer.h"
#include "block_list.h"
#include "exceptions.h"
#include "piece.h"

#include "transfer_list.h"

namespace torrent {

// TODO: Derp if transfer list isn't cleared...

TransferList::~TransferList() {
  if (!base_type::empty())
    throw internal_error("TransferList::~TransferList() called on an non-empty object");
}

TransferList::iterator
TransferList::find(uint32_t index) {
  return std::find_if(begin(), end(), [index](BlockList* b) { return index == b->index(); });
}

TransferList::const_iterator
TransferList::find(uint32_t index) const {
  return std::find_if(begin(), end(), [index](BlockList* b) { return index == b->index(); });
}

void
TransferList::clear() {
  for (const auto& block_list : *this) {
    m_slot_canceled(block_list->index());
  }
  for (const auto& block_list : *this) {
    delete block_list;
  }

  base_type::clear();
}

TransferList::iterator
TransferList::insert(const Piece& piece, uint32_t blockSize) {
  if (find(piece.index()) != end())
    throw internal_error("Delegator::new_chunk(...) received an index that is already delegated.");

  BlockList* blockList = new BlockList(piece, blockSize);
  
  m_slot_queued(piece.index());

  return base_type::insert(end(), blockList);
}

// TODO: Create a destructor to ensure all blocklists have been cleared/invaldiated?

TransferList::iterator
TransferList::erase(iterator itr) {
  if (itr == end())
    throw internal_error("TransferList::erase(...) itr == m_chunks.end().");

  delete *itr;

  return base_type::erase(itr);
}

void
TransferList::finished(BlockTransfer* transfer) {
  if (!transfer->is_valid())
    throw internal_error("TransferList::finished(...) got transfer with wrong state.");

  uint32_t index = transfer->block()->index();

  // Marks the transfer as complete and erases it.
  if (transfer->block()->completed(transfer))
    m_slot_completed(index);
}

void
TransferList::hash_succeeded(uint32_t index, Chunk* chunk) {
  iterator blockListItr = find(index);

  if (!std::all_of((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fn(&Block::is_finished)))
    throw internal_error("TransferList::hash_succeeded(...) Finished blocks does not match size.");

  // The chunk should also be marked here or by the caller so that it
  // gets priority for syncing back to disk.

  if ((*blockListItr)->failed() != 0)
    mark_failed_peers(*blockListItr, chunk);

  // Add to a list of finished chunks indices with timestamps. This is
  // mainly used for torrent resume data on which chunks need to be
  // rehashed on crashes.
  //
  // We assume the chunk gets sync'ed within 10 minutes, so minimum
  // retention time of 30 minutes should be enough. The list only gets
  // pruned every 60 minutes, so any timer that reads values once
  // every 30 minutes is guaranteed to get them all as long as it is
  // ordered properly.
  m_completedList.emplace_back(rak::timer::current().usec(), index);

  if (rak::timer(m_completedList.front().first) + rak::timer::from_minutes(60) < rak::timer::current()) {
    auto itr = std::find_if(m_completedList.begin(), m_completedList.end(), [](auto v) {
      return (rak::timer::current() - rak::timer::from_minutes(30)) <= v.first;
    });

    m_completedList.erase(m_completedList.begin(), itr);
  }

  m_succeededCount++;
  erase(blockListItr);
}

struct transfer_list_compare_data {
  transfer_list_compare_data(Chunk* chunk, const Piece& p) : m_chunk(chunk), m_piece(p) { }

  bool operator () (BlockFailed::value_type failed) {
    return m_chunk->compare_buffer(failed.first, m_piece.offset(), m_piece.length());
  }

  Chunk* m_chunk;
  Piece  m_piece;
};

void
TransferList::hash_failed(uint32_t index, Chunk* chunk) {
  iterator blockListItr = find(index);

  if (blockListItr == end())
    throw internal_error("TransferList::hash_failed(...) Could not find index.");

  if (!std::all_of((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fn(&Block::is_finished)))
    throw internal_error("TransferList::hash_failed(...) Finished blocks does not match size.");

  m_failedCount++;

  // Could propably also check promoted against size of the block
  // list.

  if ((*blockListItr)->attempt() == 0) {
    unsigned int promoted = update_failed(*blockListItr, chunk);

    if (promoted > 0 || promoted < (*blockListItr)->size()) {
      // Retry with the most popular blocks.
      (*blockListItr)->set_attempt(1);
      retry_most_popular(*blockListItr, chunk);

      // Also consider various other schemes, like using blocks from
      // only/mainly one peer.

      return;
    }
  }

  // Should we check if there's any peers whom have sent us bad data
  // before, and just clear those first?

  // Re-download the blocks.
  (*blockListItr)->do_all_failed();
}

// update_failed(...) either increments the reference count of a
// failed entry, or creates a new one if the data differs.
unsigned int
TransferList::update_failed(BlockList* blockList, Chunk* chunk) {
  unsigned int promoted = 0;

  blockList->inc_failed();

  for (auto& transfer : *blockList) {

    if (transfer.failed_list() == NULL)
      transfer.set_failed_list(new BlockFailed());

    BlockFailed::iterator failedItr = std::find_if(transfer.failed_list()->begin(), transfer.failed_list()->end(), transfer_list_compare_data(chunk, transfer.piece()));

    if (failedItr == transfer.failed_list()->end()) {
      // We've never encountered this data before, make a new entry.
      char* buffer = new char[transfer.piece().length()];

      chunk->to_buffer(buffer, transfer.piece().offset(), transfer.piece().length());

      transfer.failed_list()->emplace_back(buffer, 1);
      failedItr = transfer.failed_list()->end() - 1;

      // Count how many new data sets?

    } else {
      // Increment promoted when the entry's reference count becomes
      // larger than others, but not if it previously was the largest.

      BlockFailed::iterator maxItr = transfer.failed_list()->max_element();

      if (maxItr->second == failedItr->second && maxItr != (transfer.failed_list()->reverse_max_element().base() - 1))
        promoted++;

      failedItr->second++;
    }

    transfer.failed_list()->set_current(failedItr);
    transfer.leader()->set_failed_index(failedItr - transfer.failed_list()->begin());
  }

  return promoted;
}

void
TransferList::mark_failed_peers(BlockList* blockList, Chunk* chunk) {
  std::set<PeerInfo*> badPeers;

  for (auto& block : *blockList) {
    // This chunk data is good, set it as current and
    // everyone who sent something else is a bad peer.
    block.failed_list()->set_current(std::find_if(block.failed_list()->begin(), block.failed_list()->end(), transfer_list_compare_data(chunk, block.piece())));

    for (auto& transfer : *block.transfers())
      if (transfer->failed_index() != block.failed_list()->current() && transfer->failed_index() != ~uint32_t())
        badPeers.insert(transfer->peer_info());
  }

  std::for_each(badPeers.begin(), badPeers.end(), m_slot_corrupt);
}

// Copy the stored data to the chunk from the failed entries with
// largest reference counts.
void
TransferList::retry_most_popular(BlockList* blockList, Chunk* chunk) {
  for (auto& block : *blockList) {

    BlockFailed::reverse_iterator failedItr = block.failed_list()->reverse_max_element();

    if (failedItr == block.failed_list()->rend())
      throw internal_error("TransferList::retry_most_popular(...) No failed list entry found.");

    // The data is the same, so no need to copy.
    if (failedItr == block.failed_list()->current_reverse_iterator())
      continue;

    // Change the leader to the currently held buffer?

    chunk->from_buffer(failedItr->first, block.piece().offset(), block.piece().length());

    block.failed_list()->set_current(failedItr);
  }

  m_slot_completed(blockList->index());
}

}
