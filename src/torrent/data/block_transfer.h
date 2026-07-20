#ifndef LIBTORRENT_BLOCK_TRANSFER_H
#define LIBTORRENT_BLOCK_TRANSFER_H

#include <cassert>
#include <cstdlib>
#include <torrent/common.h>
#include <torrent/data/piece.h>
#include <torrent/peer/peer_info.h>

namespace torrent {

class LIBTORRENT_EXPORT BlockTransfer {
public:
  static constexpr uint32_t invalid_index = ~uint32_t();

  using key_type = PeerInfo*;

  enum state_type {
    STATE_ERASED,
    STATE_QUEUED,
    STATE_LEADER,
    STATE_NOT_LEADER
  };

  BlockTransfer() = default;
  ~BlockTransfer();
  BlockTransfer(const BlockTransfer&) = delete;
  BlockTransfer& operator=(const BlockTransfer&) = delete;

  // TODO: Do we need to also check for peer_info?...
  bool                is_valid() const              { return m_block != NULL; }

  bool                is_erased() const             { return m_state == STATE_ERASED; }
  bool                is_queued() const             { return m_state == STATE_QUEUED; }
  bool                is_leader() const             { return m_state == STATE_LEADER; }
  bool                is_not_leader() const         { return m_state == STATE_NOT_LEADER; }

  bool                is_finished() const           { return m_position == m_piece.length(); }

  key_type            peer_info()                   { return m_peer_info; }
  key_type            const_peer_info() const { return m_peer_info; }

  Block*              block()                       { return m_block; }
  const Block*        const_block() const           { return m_block; }

  const Piece&        piece() const                 { return m_piece; }
  uint32_t            index() const                 { return m_piece.index(); }
  state_type          state() const                 { return m_state; }
  int32_t             request_time() const          { return m_request_time; }

  // Adjust the position after any actions like erasing it from a
  // Block, but before if finishing.
  uint32_t            position() const              { return m_position; }
  uint32_t            stall() const                 { return m_stall; }
  uint32_t            failed_index() const          { return m_failedIndex; }

  void                set_peer_info(key_type p);
  void                set_block(Block* b)           { m_block = b; }
  void                set_piece(const Piece& p)     { m_piece = p; }
  void                set_state(state_type s)       { m_state = s; }
  void                set_request_time(int32_t t)   { m_request_time = t; }

  void                set_position(uint32_t p)      { m_position = p; }
  void                adjust_position(uint32_t p)   { m_position += p; }

  void                set_stall(uint32_t s)         { m_stall = s; }
  void                set_failed_index(uint32_t i)  { m_failedIndex = i; }

private:
  key_type            m_peer_info{};
  Block*              m_block{};
  Piece               m_piece;

  state_type          m_state;
  int32_t             m_request_time;

  uint32_t            m_position;
  uint32_t            m_stall;
  uint32_t            m_failedIndex;
};

inline
BlockTransfer::~BlockTransfer() {
  assert(m_block == NULL && "BlockTransfer::~BlockTransfer() block not NULL");
  assert(m_peer_info == NULL && "BlockTransfer::~BlockTransfer() peer_info not NULL");
}

inline void
BlockTransfer::set_peer_info(key_type p) {
  if (m_peer_info != NULL)
    m_peer_info->dec_transfer_counter();

  m_peer_info = p;

  if (m_peer_info != NULL)
    m_peer_info->inc_transfer_counter();
}

} // namespace torrent

#endif
