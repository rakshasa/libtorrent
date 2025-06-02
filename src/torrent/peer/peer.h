#ifndef LIBTORRENT_PEER_H
#define LIBTORRENT_PEER_H

#include <string>
#include <torrent/common.h>
#include <torrent/peer/peer_info.h>

namespace torrent {

class PeerConnectionBase;

// == and = operators works as expected.

// The Peer class is a wrapper around the internal peer class. This
// peer class may be invalidated during a torrent::work call. So if
// you keep a list or single instances in the client, you need to
// listen to the appropriate signals from the download to keep up to
// date.

class LIBTORRENT_EXPORT Peer {
public:
  virtual ~Peer() = default;
  Peer(const Peer&) = delete;
  Peer& operator=(const Peer&) = delete;

  // Does not check if it has been removed from the download.
  bool                 is_incoming() const                { return peer_info()->is_incoming(); }
  bool                 is_encrypted() const;
  bool                 is_obfuscated() const;

  bool                 is_up_choked() const;
  bool                 is_up_interested() const;

  bool                 is_down_choked() const;
  bool                 is_down_choked_limited() const;
  bool                 is_down_queued() const;
  bool                 is_down_interested() const;

  bool                 is_snubbed() const;
  bool                 is_banned() const;

  void                 set_snubbed(bool v);
  void                 set_banned(bool v);

  const HashString&    id() const                         { return peer_info()->id(); }
  const char*          options() const                    { return peer_info()->options(); }
  const sockaddr*      address() const                    { return peer_info()->socket_address(); }

  const Rate*          down_rate() const;
  const Rate*          up_rate() const;
  const Rate*          peer_rate() const;

  const Bitfield*      bitfield() const;

  const BlockTransfer* transfer() const;

  uint32_t             incoming_queue_size() const;
  uint32_t             outgoing_queue_size() const;

  uint32_t             chunks_done() const;

  uint32_t             failed_counter() const             { return peer_info()->failed_counter(); }

  void                 disconnect(int flags);

  //
  // New interface:
  //

  const PeerInfo*      peer_info() const                  { return m_peerInfo; }

  PeerConnectionBase*       m_ptr()       { return reinterpret_cast<PeerConnectionBase*>(this); }
  const PeerConnectionBase* c_ptr() const { return reinterpret_cast<const PeerConnectionBase*>(this); }

protected:
  Peer() = default;

  bool                 operator == (const Peer& p) const;

  PeerInfo*            m_peerInfo;
};

} // namespace torrent

#endif
