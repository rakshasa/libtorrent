#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H

#include <cstdint>

#include "data/chunk_handle.h"
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

struct StateBase {
  choke_status choke;
  ChunkHandle  chunk;
};

struct StateUp : StateBase {
  Piece piece;
};

struct StateDown : StateBase {
  uint32_t stall{};

  bool interested{};
  bool unchoked{};

  // NOT_INTERESTED sent by a download choke, not by the pieces.
  bool interest_withheld{};
};

} // namespace torrent::protocol

#endif
