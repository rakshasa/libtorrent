// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
  Peer() {}
  virtual ~Peer() {}

  Peer(const Peer&);
  void operator = (const Peer&);

  bool                 operator == (const Peer& p) const;

  PeerInfo*            m_peerInfo;
};

}

#endif
