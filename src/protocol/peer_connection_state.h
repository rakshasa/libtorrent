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
  static constexpr int PEX_DO      = (1 << 0);
  static constexpr int PEX_ENABLE  = (1 << 1);
  static constexpr int PEX_DISABLE = (1 << 2);

  bool                is_choked() const;
  bool                is_interested() const;
  bool                is_snubbed() const;

  bool                has_pending_choke() const;
  void                schedule_choke();
  void                clear_pending_choke();

  bool                has_pending_interest() const;
  void                schedule_interest();
  void                clear_pending_interest();

  bool                has_pending_pex() const;
  bool                has_pending_pex_toggle() const;
  bool                has_pending_pex_enable() const;
  bool                has_pending_pex_exchange() const;
  void                schedule_pex_exchange();
  void                schedule_pex_enable();
  void                schedule_pex_disable();
  void                clear_pending_pex_toggle();
  void                clear_pending_pex_exchange();
  void                clear_pending_pex();

  Piece               piece;

private:
  bool                m_send_choked{};
  bool                m_send_interested{};
  int                 m_send_pex_mask{};
};

inline bool StateUp::is_choked() const                { return choke.choked(); }
inline bool StateUp::is_interested() const            { return choke.queued(); }
inline bool StateUp::is_snubbed() const               { return choke.snubbed(); }

inline bool StateUp::has_pending_choke() const        { return m_send_choked; }
inline void StateUp::schedule_choke()                 { m_send_choked = true; }
inline void StateUp::clear_pending_choke()            { m_send_choked = false; }

inline bool StateUp::has_pending_interest() const     { return m_send_interested; }
inline void StateUp::schedule_interest()              { m_send_interested = true; }
inline void StateUp::clear_pending_interest()         { m_send_interested = false; }

inline bool StateUp::has_pending_pex() const          { return m_send_pex_mask != 0; }
inline bool StateUp::has_pending_pex_toggle() const   { return m_send_pex_mask & (PEX_ENABLE | PEX_DISABLE); }
inline bool StateUp::has_pending_pex_enable() const   { return m_send_pex_mask & PEX_ENABLE; }
inline bool StateUp::has_pending_pex_exchange() const { return m_send_pex_mask & PEX_DO; }
inline void StateUp::schedule_pex_exchange()          { m_send_pex_mask |= PEX_DO; }
inline void StateUp::schedule_pex_enable()            { m_send_pex_mask = PEX_ENABLE | (m_send_pex_mask & ~PEX_DISABLE); }
inline void StateUp::schedule_pex_disable()           { m_send_pex_mask = PEX_DISABLE | (m_send_pex_mask & ~PEX_ENABLE); }
inline void StateUp::clear_pending_pex_toggle()       { m_send_pex_mask &= ~(PEX_ENABLE | PEX_DISABLE); }
inline void StateUp::clear_pending_pex_exchange()     { m_send_pex_mask &= ~PEX_DO; }
inline void StateUp::clear_pending_pex()              { m_send_pex_mask = 0; }

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
