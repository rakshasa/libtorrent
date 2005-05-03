// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_NET_PEER_CONNECTION_BASE_H
#define LIBTORRENT_NEW_PEER_CONNECTION_BASE_H

#include "data/piece.h"
#include "net/poll.h"
#include "net/protocol_buffer.h"
#include "net/protocol_read.h"
#include "net/protocol_write.h"
#include "net/socket_base.h"
#include "utils/bitfield_ext.h"
#include "utils/rate.h"
#include "utils/task.h"
#include "utils/throttle.h"

#include "peer_info.h"

namespace torrent {

// Base class for peer connection classes. Rename to PeerConnection
// when the migration is complete.

class DownloadState;
class DownloadNet;

class PeerConnectionBase : public SocketBase {
public:
  PeerConnectionBase();
  ~PeerConnectionBase();
  
  bool                is_write_choked()             { return m_write.get_choked(); }
  bool                is_write_interested()         { return m_write.get_interested(); }
  bool                is_read_choked()              { return m_read.get_choked(); }
  bool                is_read_interested()          { return m_read.get_interested(); }

  bool                is_read_throttled()           { return m_readThrottle != throttleRead.end(); }
  bool                is_write_throttled()          { return m_writeThrottle != throttleWrite.end(); }

  bool                is_snubbed() const            { return m_snubbed; }

  const PeerInfo&     get_peer() const              { return m_peer; }

  Rate&               get_rate_peer()               { return m_ratePeer; }
  Rate&               get_rate_up()                 { return m_rateUp; }
  Rate&               get_rate_down()               { return m_rateDown; }

  Timer               get_last_choked()             { return m_lastChoked; }

  const BitFieldExt&  get_bitfield() const          { return m_bitfield; }

  // Make sure you choke the peer when snubbing. Snubbing a peer will
  // only cause it not to be unchoked.
  void                set_snubbed(bool v)           { m_snubbed = v; }

  void                insert_read_throttle();
  void                remove_read_throttle();

  void                insert_write_throttle();
  void                remove_write_throttle();

protected:
  inline bool         read_remaining();
  inline bool         write_remaining();

  void                load_read_chunk(const Piece& p);

  void                receive_throttle_read_activate();
  void                receive_throttle_write_activate();

  DownloadState*      m_state;
  DownloadNet*        m_net;

  PeerInfo            m_peer;
   
  Rate                m_ratePeer;
  Rate                m_rateUp;
  Rate                m_rateDown;

  ThrottlePeerNode    m_readThrottle;
  ThrottlePeerNode    m_writeThrottle;

  bool                m_snubbed;

  BitFieldExt         m_bitfield;
   
  Piece               m_readPiece;
  Piece               m_writePiece;

  Timer               m_lastChoked;

  ProtocolRead        m_read;
  ProtocolWrite       m_write;
};

inline void
PeerConnectionBase::insert_read_throttle() {
  if (!is_read_throttled())
    m_readThrottle = throttleRead.insert(PeerConnectionThrottle(this, &PeerConnectionBase::receive_throttle_read_activate));
}

inline void
PeerConnectionBase::remove_read_throttle() {
  if (is_read_throttled()) {
    throttleRead.erase(m_readThrottle);
    m_readThrottle = throttleRead.end();
  }
}

inline void
PeerConnectionBase::insert_write_throttle() {
  if (!is_write_throttled())
    m_writeThrottle = throttleWrite.insert(PeerConnectionThrottle(this, &PeerConnectionBase::receive_throttle_write_activate));
}

inline void
PeerConnectionBase::remove_write_throttle() {
  if (is_write_throttled()) {
    throttleWrite.erase(m_writeThrottle);
    m_writeThrottle = throttleWrite.end();
  }
}

inline bool
PeerConnectionBase::read_remaining() {
  m_read.get_buffer().move_position(read_buf(m_read.get_buffer().position(),
					     m_read.get_buffer().remaining()));

  return !m_read.get_buffer().remaining();
}

inline bool
PeerConnectionBase::write_remaining() {
  m_write.get_buffer().move_position(write_buf(m_write.get_buffer().position(),
					       m_write.get_buffer().remaining()));

  return !m_write.get_buffer().remaining();
}

}

#endif
