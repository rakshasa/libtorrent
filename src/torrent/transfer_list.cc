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
#include <set>
#include <rak/functional.h>

#include "data/chunk.h"

#include "block_failed.h"
#include "block_transfer.h"
#include "block_list.h"
#include "exceptions.h"
#include "peer_info.h"
#include "piece.h"

#include "transfer_list.h"

namespace torrent {

TransferList::iterator
TransferList::find(uint32_t index) {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

TransferList::const_iterator
TransferList::find(uint32_t index) const {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

void
TransferList::clear() {
  std::for_each(begin(), end(), rak::on(std::mem_fun(&BlockList::index), m_slotCanceled));
  std::for_each(begin(), end(), rak::call_delete<BlockList>());

  base_type::clear();
}

TransferList::iterator
TransferList::insert(const Piece& piece, uint32_t blockSize) {
  if (find(piece.index()) != end())
    throw internal_error("Delegator::new_chunk(...) received an index that is already delegated.");

  BlockList* blockList = new BlockList(piece, blockSize);
  
  m_slotQueued(piece.index());

  return base_type::insert(end(), blockList);
}

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
    m_slotCompleted(index);
}

void
TransferList::hash_succeded(uint32_t index) {
  iterator blockListItr = find(index);

  if ((Block::size_type)std::count_if((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::is_finished)) != (*blockListItr)->size())
    throw internal_error("TransferList::hash_succeded(...) Finished blocks does not match size.");

  if ((*blockListItr)->failed() != 0)
    mark_failed_peers(*blockListItr);

  erase(blockListItr);
}

struct transfer_list_compare_data {
  transfer_list_compare_data(Chunk* chunk, const Piece& p) : m_chunk(chunk), m_piece(p) { }

  bool operator () (const BlockFailed::reference failed) {
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

  if ((Block::size_type)std::count_if((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::is_finished)) != (*blockListItr)->size())
    throw internal_error("TransferList::hash_failed(...) Finished blocks does not match size.");

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
  (*blockListItr)->clear_finished();
  (*blockListItr)->set_attempt(0);

  // Clear leaders when we want to redownload the chunk.
  std::for_each((*blockListItr)->begin(), (*blockListItr)->end(), std::mem_fun_ref(&Block::failed_leader));
}

// update_failed(...) either increments the reference count of a
// failed entry, or creates a new one if the data differs.
unsigned int
TransferList::update_failed(BlockList* blockList, Chunk* chunk) {
  unsigned int promoted = 0;

  blockList->inc_failed();

  for (BlockList::iterator itr = blockList->begin(), last = blockList->end(); itr != last; ++itr) {
    
    if (itr->failed_list() == NULL)
      itr->set_failed_list(new BlockFailed());

    BlockFailed::iterator failedItr = std::find_if(itr->failed_list()->begin(), itr->failed_list()->end(),
                                                   transfer_list_compare_data(chunk, itr->piece()));

    if (failedItr == itr->failed_list()->end()) {
      // We've never encountered this data before, make a new entry.
      char* buffer = new char[itr->piece().length()];

      chunk->to_buffer(buffer, itr->piece().offset(), itr->piece().length());

      itr->failed_list()->push_back(BlockFailed::value_type(buffer, 1));

      // Count how many new data sets?

    } else {
      // Increment promoted when the entry's reference count becomes
      // larger than others, but not if it previously was the largest.

      BlockFailed::iterator maxItr = itr->failed_list()->max_element();

      if (maxItr->second == failedItr->second && maxItr != --itr->failed_list()->reverse_max_element().base())
        promoted++;

      failedItr->second++;
    }

    itr->failed_list()->set_current(failedItr);
    itr->leader()->set_failed_index(failedItr - itr->failed_list()->begin());
  }

  return promoted;
}

void
TransferList::mark_failed_peers(BlockList* blockList) {
  std::set<PeerInfo*> badPeers;

  for (BlockList::iterator itr = blockList->begin(), last = blockList->end(); itr != last; ++itr)
    for (Block::transfer_list_type::const_iterator itr2 = itr->transfers()->begin(), last2 = itr->transfers()->end(); itr2 != last2; ++itr2)
      if ((*itr2)->failed_index() != itr->failed_list()->current())
        badPeers.insert((*itr2)->peer_info());

  std::for_each(badPeers.begin(), badPeers.end(), m_slotCorrupt);
}

// Copy the stored data to the chunk from the failed entries with
// largest reference counts.
void
TransferList::retry_most_popular(BlockList* blockList, Chunk* chunk) {
  for (BlockList::iterator itr = blockList->begin(), last = blockList->end(); itr != last; ++itr) {
    
    BlockFailed::reverse_iterator failedItr = itr->failed_list()->reverse_max_element();

    if (failedItr == itr->failed_list()->rend())
      throw internal_error("TransferList::retry_most_popular(...) No failed list entry found.");

    // The data is the same, so no need to copy.
    if (failedItr == itr->failed_list()->current_reverse_iterator())
      continue;

    // Change the leader to the currently held buffer?

    chunk->from_buffer(failedItr->first, itr->piece().offset(), itr->piece().length());

    itr->failed_list()->set_current(failedItr);
  }

  m_slotCompleted(blockList->index());
}

}
