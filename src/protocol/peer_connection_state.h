#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_STATE_H

#include <chrono>
#include <cstdint>
#include <cstring>
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
  int                 m_send_pex_mask{};
};

struct StateDown : StateBase {

  bool                is_interested() const;
  bool                is_interest_withheld() const;
  bool                is_unchoked() const;
  bool                is_local_unchoked() const;
  bool                is_queued() const;

  bool                restore_interest(bool download_done);
  void                keep_interest();

  void                mark_interested();
  void                mark_not_interested();
  void                mark_interested_and_schedule();
  void                mark_not_interested_and_schedule();
  void                mark_interested_and_schedule_if_changed();

  void                withhold_interest_and_schedule();
  void                clear_interest_withheld();

  bool                has_pending_interest() const;
  void                clear_pending_interest();

  void                mark_choked();
  void                mark_unchoked();

  void                push_unread(const void* data, uint32_t size) const;

  // How long the peer has been of no use to the download: we want its data, it did not unchoke us, and no piece data
  // went either way (down_total, up_total: the connection's piece bytes so far). A peer that unchoked us while our own
  // download slots keep us from asking it, or that wants our pieces, is not charged: the wait is ours. Moved on every
  // keepalive pass; a wait counts from the first pass that saw it.
  void                update_idle_time(const StateUp& up, uint64_t down_total, uint64_t up_total, std::chrono::seconds now);
  std::chrono::seconds idle_time() const;

  // A zero limit is never reached (DownloadInfo::idle_peer_limit).
  bool                is_idle_for(std::chrono::seconds limit) const;

  bool                has_pending_request() const;
  void                schedule_request();
  void                clear_pending_request();
  void                set_pending_request(bool pending);

  uint32_t            stall{};

private:
  bool                m_request_pending{};
  bool                m_interested{};
  bool                m_unchoked{};
  bool                m_send_interested{};

  // NOT_INTERESTED sent by a download choke, not by the pieces.
  bool                m_interest_withheld{};

  std::chrono::seconds m_idle_seen{};
  std::chrono::seconds m_idle_time{};
  uint64_t            m_idle_down_total{};
  uint64_t            m_idle_up_total{};
  bool                m_idle_was_waiting{};   // It kept us waiting at the last look.
  bool                m_idle_was_unchoked{};  // It unchoked us since the last look.
};

inline bool StateUp::is_choked() const                    { return choke.choked(); }
inline bool StateUp::is_interested() const                { return choke.queued(); }
inline bool StateUp::is_snubbed() const                   { return choke.snubbed(); }

inline bool StateUp::has_pending_choke() const            { return m_send_choked; }
inline void StateUp::schedule_choke()                     { m_send_choked = true; }
inline void StateUp::clear_pending_choke()                { m_send_choked = false; }

inline bool StateUp::has_pending_pex() const              { return m_send_pex_mask != 0; }
inline bool StateUp::has_pending_pex_toggle() const       { return m_send_pex_mask & (PEX_ENABLE | PEX_DISABLE); }
inline bool StateUp::has_pending_pex_enable() const       { return m_send_pex_mask & PEX_ENABLE; }
inline bool StateUp::has_pending_pex_exchange() const     { return m_send_pex_mask & PEX_DO; }
inline void StateUp::schedule_pex_exchange()              { m_send_pex_mask |= PEX_DO; }
inline void StateUp::schedule_pex_enable()                { m_send_pex_mask = PEX_ENABLE | (m_send_pex_mask & ~PEX_DISABLE); }
inline void StateUp::schedule_pex_disable()               { m_send_pex_mask = PEX_DISABLE | (m_send_pex_mask & ~PEX_ENABLE); }
inline void StateUp::clear_pending_pex_toggle()           { m_send_pex_mask &= ~(PEX_ENABLE | PEX_DISABLE); }
inline void StateUp::clear_pending_pex_exchange()         { m_send_pex_mask &= ~PEX_DO; }
inline void StateUp::clear_pending_pex()                  { m_send_pex_mask = 0; }

inline bool StateDown::is_interested() const              { return m_interested; }
inline bool StateDown::is_interest_withheld() const       { return m_interest_withheld; }
inline bool StateDown::is_unchoked() const                { return m_unchoked; }
inline bool StateDown::is_local_unchoked() const          { return choke.unchoked(); }
inline bool StateDown::is_queued() const                  { return choke.queued(); }

inline void StateDown::mark_interested()                  { m_interested = true; }
inline void StateDown::mark_not_interested()              { m_interested = false; }
inline void StateDown::mark_interested_and_schedule()     { m_interested = true; m_send_interested = true; }
inline void StateDown::mark_not_interested_and_schedule() { m_interested = false; m_send_interested = true; }
inline void StateDown::clear_interest_withheld()          { m_interest_withheld = false; }
inline bool StateDown::has_pending_interest() const       { return m_send_interested; }
inline void StateDown::clear_pending_interest()           { m_send_interested = false; }

inline void StateDown::mark_choked()                      { m_unchoked = false; }
inline void StateDown::mark_unchoked()                    { m_unchoked = true; m_idle_was_unchoked = true; }
inline bool StateDown::has_pending_request() const        { return m_request_pending; }
inline void StateDown::schedule_request()                 { m_request_pending = true; }
inline void StateDown::clear_pending_request()            { m_request_pending = false; }
inline void StateDown::set_pending_request(bool pending)  { m_request_pending = pending; }

inline std::chrono::seconds StateDown::idle_time() const  { return m_idle_time; }
inline bool StateDown::is_idle_for(std::chrono::seconds limit) const { return limit != std::chrono::seconds{} && m_idle_time >= limit; }

// Restore withheld interest unless the download is complete.
inline bool StateDown::restore_interest(bool download_done) {
  if (!is_interest_withheld())
    return false;

  clear_interest_withheld();

  if (download_done || is_interested())
    return false;

  mark_interested_and_schedule();
  return true;
}

// Keep interest while a rotated download slot is unavailable.
inline void StateDown::keep_interest() {
  if (!is_interest_withheld())
    return;

  clear_interest_withheld();
  mark_interested();
  clear_pending_interest();
}

inline void StateDown::push_unread(const void* data, uint32_t size) const {
  std::memcpy(protocol->buffer()->end(), data, size);
  protocol->buffer()->move_end(size);
}

inline void StateDown::mark_interested_and_schedule_if_changed() {
  m_send_interested = !m_interested;
  m_interested = true;
}

inline void StateDown::update_idle_time(const StateUp& up, uint64_t down_total, uint64_t up_total, std::chrono::seconds now) {
  auto elapsed     = now - m_idle_seen;
  bool ours        = (is_unchoked() && !is_local_unchoked()) || up.is_interested();
  bool was_waiting = m_idle_was_waiting;

  m_idle_seen        = now;
  m_idle_was_waiting = is_interested() && !ours;

  if (!is_interested() || down_total != m_idle_down_total || up_total != m_idle_up_total || m_idle_was_unchoked) {
    m_idle_down_total   = down_total;
    m_idle_up_total     = up_total;
    m_idle_was_unchoked = false;
    m_idle_time         = std::chrono::seconds{};
    return;
  }

  // Two minutes pass between looks, so only time that began with the peer keeping us waiting counts.
  if (m_idle_was_waiting && was_waiting)
    m_idle_time += elapsed;
}

inline void StateDown::withhold_interest_and_schedule() {
  m_interest_withheld = m_interested;
  m_send_interested   = m_interested;
  m_interested        = false;
}

} // namespace torrent::protocol

#endif
