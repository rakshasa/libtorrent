#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H

#include <cstdint>
#include <memory>

#include "data/chunk_handle.h"
#include "protocol/protocol_base.h"
#include "torrent/data/piece.h"
#include "torrent/peer/choke_status.h"

namespace torrent::protocol {

// The interested state no longer follows the spec's wording as it has been
// swapped.
//
// For the up state 'interested' means the remote peer wants upload and 'choke'
// means we've choked upload to that peer.
//
// For the down state, 'queued' now means the same as the spec's 'unchoked',
// while 'unchoked' means we start requesting pieces.

using ProtocolRead  = ProtocolBase;
using ProtocolWrite = ProtocolBase;

struct StateBase {
  choke_status choke;
  ChunkHandle  chunk;
};

struct StateUp : StateBase {
  std::unique_ptr<ProtocolWrite> protocol{std::make_unique<ProtocolWrite>()};

  Piece piece;
};

struct StateDown : StateBase {
  std::unique_ptr<ProtocolRead> protocol{std::make_unique<ProtocolRead>()};

  uint32_t stall{};

  bool is_interested_in_remote_peer() const { return m_interested; }
  void mark_interested_in_remote_peer() { m_interested = true; }
  void mark_not_interested_in_remote_peer() { m_interested = false; }

  bool is_unchoked_by_remote_peer() const { return m_unchoked_by_remote_peer; }
  void mark_unchoked_by_remote_peer() { m_unchoked_by_remote_peer = true; }
  void mark_choked_by_remote_peer() { m_unchoked_by_remote_peer = false; }

  bool is_interest_withheld_from_remote_peer() const { return m_interest_withheld; }
  bool withhold_interest_from_remote_peer() {
    m_interest_withheld = m_interested;
    m_interested = false;
    return m_interest_withheld;
  }
  void clear_interest_withheld_from_remote_peer() { m_interest_withheld = false; }

private:
  bool m_interested{};
  bool m_unchoked_by_remote_peer{};

  // NOT_INTERESTED sent by a download choke, not by the pieces.
  bool m_interest_withheld{};
};

} // namespace torrent::protocol

#endif
