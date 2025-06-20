#ifndef LIBTORRENT_BLOCK_H
#define LIBTORRENT_BLOCK_H

#include <cstdlib>
#include <torrent/common.h>
#include <torrent/data/piece.h>
#include <vector>

namespace torrent {

// If you start adding slots, make sure the rest of the code creates
// copies and clears the original variables before calls to erase etc.

class LIBTORRENT_EXPORT Block {
public:
  // Using vectors as they will remain small, thus the cost of erase
  // should be small. Later we can do faster erase by ignoring the
  // ordering.
  using transfer_list_type = std::vector<BlockTransfer*>;
  using size_type          = uint32_t;

  enum state_type {
    STATE_INCOMPLETE,
    STATE_COMPLETED,
    STATE_INVALID
  };

  Block() = default;
  ~Block();

  // Only allow move constructions
  Block(const Block&) = delete;
  Block& operator=(const Block&) = delete;
  Block(Block&&) = default;
  Block& operator=(Block&&) = default;

  bool                      is_stalled() const                           { return m_notStalled == 0; }
  bool                      is_finished() const;
  bool                      is_transfering() const;

  bool                      is_peer_queued(const PeerInfo* p) const      { return find_queued(p) != NULL; }
  bool                      is_peer_transfering(const PeerInfo* p) const { return find_transfer(p) != NULL; }

  size_type                 size_all() const                             { return m_queued.size() + m_transfers.size(); }
  size_type                 size_not_stalled() const                     { return m_notStalled; }

  BlockList*                parent()                                     { return m_parent; }
  const BlockList*          parent() const                               { return m_parent; }
  void                      set_parent(BlockList* p)                     { m_parent = p; }

  const Piece&              piece() const                                { return m_piece; }
  void                      set_piece(const Piece& p)                    { m_piece = p; }

  uint32_t                  index() const                                { return m_piece.index(); }

  const transfer_list_type* queued() const                               { return &m_queued; }
  const transfer_list_type* transfers() const                            { return &m_transfers; }

  // The leading transfer, whom's data we're currently using.
  BlockTransfer*            leader()                                     { return m_leader; }
  const BlockTransfer*      leader() const                               { return m_leader; }

  BlockTransfer*            find(const PeerInfo* p);
  const BlockTransfer*      find(const PeerInfo* p) const;

  BlockTransfer*            find_queued(const PeerInfo* p);
  const BlockTransfer*      find_queued(const PeerInfo* p) const;

  BlockTransfer*            find_transfer(const PeerInfo* p);
  const BlockTransfer*      find_transfer(const PeerInfo* p) const;

  // Internal to libTorrent:

  BlockTransfer*            insert(PeerInfo* peerInfo);
  void                      erase(BlockTransfer* transfer);

  bool                      transfering(BlockTransfer* transfer);
  void                      transfer_dissimilar(BlockTransfer* transfer);

  bool                      completed(BlockTransfer* transfer);
  void                      retry_transfer();

  static void               stalled(BlockTransfer* transfer);
  void                      stalled_transfer(BlockTransfer* transfer);

  void                      change_leader(BlockTransfer* transfer);
  void                      failed_leader();

  BlockFailed*              failed_list()                                { return m_failedList; }
  void                      set_failed_list(BlockFailed* f)              { m_failedList = f; }

  static void               create_dummy(BlockTransfer* transfer, PeerInfo* peerInfo, const Piece& piece);

  // If the queued or transfering is already removed from the block it
  // will just delete the object. Made static so it can be called when
  // block == NULL.
  static void               release(BlockTransfer* transfer);
private:

  void                      invalidate_transfer(BlockTransfer* transfer) LIBTORRENT_NO_EXPORT;

  void                      remove_erased_transfers() LIBTORRENT_NO_EXPORT;
  void                      remove_non_leader_transfers() LIBTORRENT_NO_EXPORT;

  BlockList*                m_parent;
  Piece                     m_piece;

  state_type                m_state{STATE_INCOMPLETE};
  uint32_t                  m_notStalled{0};

  transfer_list_type        m_queued;
  transfer_list_type        m_transfers;

  BlockTransfer*            m_leader{};

  BlockFailed*              m_failedList{};
};

inline BlockTransfer*
Block::find(const PeerInfo* p) {
  if (auto transfer = find_queued(p))
    return transfer;
  else
    return find_transfer(p);
}

inline const BlockTransfer*
Block::find(const PeerInfo* p) const {
  if (auto transfer = find_queued(p))
    return transfer;
  else
    return find_transfer(p);
}

} // namespace torrent

#endif
