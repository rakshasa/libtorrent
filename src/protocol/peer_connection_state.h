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

struct StateBase {
  using protocol_ptr = std::unique_ptr<ProtocolBase>;

  choke_status        choke;
  ChunkHandle         chunk;

  protocol_ptr        protocol{std::make_unique<ProtocolBase>()};
};

struct StateUp : StateBase {
  Piece               piece;
};

struct StateDown : StateBase {
  bool                is_interested() const;
  bool                is_interest_withheld() const;
  bool                is_unchoked() const;

  void                mark_interested();
  void                mark_not_interested();

  bool                withhold_interest();
  void                clear_interest_withheld();

  void                mark_choked();
  void                mark_unchoked();

  uint32_t            stall{};

private:
  bool                m_interested{};
  bool                m_unchoked{};

  // NOT_INTERESTED sent by a download choke, not by the pieces.
  bool                m_interest_withheld{};
};

inline bool StateDown::is_interested() const        { return m_interested; }
inline bool StateDown::is_interest_withheld() const { return m_interest_withheld; }
inline bool StateDown::is_unchoked() const          { return m_unchoked; }

inline void StateDown::mark_interested()            { m_interested = true; }
inline void StateDown::mark_not_interested()        { m_interested = false; }
inline void StateDown::clear_interest_withheld()    { m_interest_withheld = false; }

inline void StateDown::mark_choked()                { m_unchoked = false; }
inline void StateDown::mark_unchoked()              { m_unchoked = true; }

inline bool StateDown::withhold_interest() {
  m_interest_withheld = m_interested;
  m_interested        = false;
  return m_interest_withheld;
}

} // namespace torrent::protocol

#endif
