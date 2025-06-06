#include "config.h"

#include "torrent/bitfield.h"
#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/data/block_transfer.h"
#include "protocol/peer_chunks.h"

#include "delegator.h"

namespace torrent {

std::vector<BlockTransfer*>
Delegator::delegate(PeerChunks* peerChunks, uint32_t affinity, uint32_t maxPieces) {
  // TODO: Make sure we don't queue the same piece several time on the same peer when
  // it timeout cancels them.
  std::vector<BlockTransfer*> new_transfers;
  PeerInfo* peerInfo = peerChunks->peer_info();

  // Find piece with same index as affinity. This affinity should ensure that we
  // never start another piece while the chunk this peer used to download is still
  // in progress.

  // TODO: What if the hash failed? Don't want data from that peer again.
  if (affinity >= 0) {
    for (BlockList* itr : m_transfers) {
      if (new_transfers.size() >= maxPieces)
        return new_transfers;

      if (affinity == itr->index())
        delegate_from_blocklist(new_transfers, maxPieces, itr, peerInfo);
    }
  }

  // Prioritize full seeders
  if (peerChunks->is_seeder()) {
    for (BlockList* itr : m_transfers) {
      if (new_transfers.size() >= maxPieces)
        return new_transfers;

      if (itr->by_seeder())
        delegate_from_blocklist(new_transfers, maxPieces, itr, peerInfo);
    }

    // Create new high priority pieces.
    delegate_new_chunks(new_transfers, maxPieces, peerChunks, true);
    // Create new normal priority pieces.
    delegate_new_chunks(new_transfers, maxPieces, peerChunks, false);
  }

  if (new_transfers.size() >= maxPieces)
    return new_transfers;

  // Find existing high priority pieces.
  for (BlockList* itr : m_transfers) {
    if (new_transfers.size() >= maxPieces)
      return new_transfers;

    if (itr->priority() == PRIORITY_HIGH && peerChunks->bitfield()->get(itr->index()))
      delegate_from_blocklist(new_transfers, maxPieces, itr, peerInfo);
  }

  // Create new high priority pieces.
  delegate_new_chunks(new_transfers, maxPieces, peerChunks, true);

  // Find existing normal priority pieces.
  for (BlockList* itr : m_transfers) {
    if (new_transfers.size() >= maxPieces)
      return new_transfers;

    if (itr->priority() == PRIORITY_NORMAL && peerChunks->bitfield()->get(itr->index()))
      delegate_from_blocklist(new_transfers, maxPieces, itr, peerInfo);
  }

  // Create new normal priority pieces.
  delegate_new_chunks(new_transfers, maxPieces, peerChunks, false);

  if (!m_aggressive)
    return new_transfers;

  // In aggressive mode, look for possible downloads that already have
  // one or more transfers queued.

  // No more than 4 per piece.
  uint16_t overlapped = 5;

  for (BlockList* itr : m_transfers) {
    if (new_transfers.size() >= maxPieces)
      return new_transfers;

    if (peerChunks->bitfield()->get(itr->index()) && itr->priority() != PRIORITY_OFF) {
      for (auto bl_itr = itr->begin(); bl_itr != itr->end() && overlapped != 0; bl_itr++) {
        if (new_transfers.size() >= maxPieces || bl_itr->size_not_stalled() >= overlapped)
          break;

        if (!bl_itr->is_finished()) {
          BlockTransfer* inserted_info = bl_itr->insert(peerInfo);
          if (inserted_info != NULL) {
            new_transfers.push_back(inserted_info);
            overlapped = bl_itr->size_not_stalled();
          }
        }
      }
    }
  }

  return new_transfers;
}

void
Delegator::delegate_new_chunks(std::vector<BlockTransfer*> &transfers, uint32_t maxPieces, PeerChunks* pc, bool highPriority) {
  // Find new chunks and if successful, add all possible pieces into `transfers`
  while (transfers.size() < maxPieces) {
    uint32_t index = m_slot_chunk_find(pc, highPriority);

    if (index == ~uint32_t{0})
      return;

    auto itr = m_transfers.insert(Piece(index, 0, m_slot_chunk_size(index)), block_size);

    (*itr)->set_by_seeder(pc->is_seeder());

    if (highPriority)
      (*itr)->set_priority(PRIORITY_HIGH);
    else
      (*itr)->set_priority(PRIORITY_NORMAL);

    delegate_from_blocklist(transfers, maxPieces, *itr, pc->peer_info());
  }
}

void
Delegator::delegate_from_blocklist(std::vector<BlockTransfer*> &transfers, uint32_t maxPieces, BlockList* c, PeerInfo* peerInfo) {
  for (auto i = c->begin(); i != c->end() && transfers.size() < maxPieces; ++i) {
    // If not finished and stalled, and no one is downloading this, then assign
    if (!i->is_finished() && i->is_stalled() && i->size_all() == 0)
      transfers.push_back(i->insert(peerInfo));
  }

  if (transfers.size() >= maxPieces)
    return;

  // Fill any remaining slots with potentially stalled pieces.
  for (auto i = c->begin(); i != c->end() && transfers.size() < maxPieces; ++i) {
    if (!i->is_finished() && i->is_stalled()) {
      BlockTransfer* inserted_info = i->insert(peerInfo);
      if (inserted_info != NULL)
        transfers.push_back(inserted_info);
    }
  }
}

} // namespace torrent
