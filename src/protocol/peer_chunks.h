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

#ifndef LIBTORRENT_PROTOCOL_PEER_CHUNKS_H
#define LIBTORRENT_PROTOCOL_PEER_CHUNKS_H

#include <list>
#include <rak/partial_queue.h>
#include <rak/timer.h>

#include "net/throttle_node.h"
#include "torrent/bitfield.h"
#include "torrent/data/piece.h"
#include "torrent/rate.h"

namespace torrent {

class PeerInfo;

class PeerChunks {
public:
  typedef std::list<Piece>    piece_list_type;

  bool                is_seeder() const             { return m_bitfield.is_all_set(); }

  PeerInfo*           peer_info()                   { return m_peerInfo; }
  const PeerInfo*     peer_info() const             { return m_peerInfo; }
  void                set_peer_info(PeerInfo* p)    { m_peerInfo = p; }

  bool                using_counter() const         { return m_usingCounter; }
  void                set_using_counter(bool state) { m_usingCounter = state; }

  Bitfield*           bitfield()                    { return &m_bitfield; }
  const Bitfield*     bitfield() const              { return &m_bitfield; }

  rak::partial_queue* download_cache()              { return &m_downloadCache; }

  piece_list_type*       upload_queue()             { return &m_uploadQueue; }
  const piece_list_type* upload_queue() const       { return &m_uploadQueue; }
  piece_list_type*       cancel_queue()             { return &m_cancelQueue; }

  // Timer used to figure out what HAVE_PIECE messages have not been
  // sent.
  rak::timer          have_timer() const            { return m_haveTimer; }
  void                set_have_timer(rak::timer t)  { m_haveTimer = t; }

  Rate*               peer_rate()                   { return &m_peerRate; }
  const Rate*         peer_rate() const             { return &m_peerRate; }

  ThrottleNode*       download_throttle()           { return &m_downloadThrottle; }
  const ThrottleNode* download_throttle() const     { return &m_downloadThrottle; }
  ThrottleNode*       upload_throttle()             { return &m_uploadThrottle; }
  const ThrottleNode* upload_throttle() const       { return &m_uploadThrottle; }

private:
  PeerInfo*           m_peerInfo{};

  bool                m_usingCounter{false};

  Bitfield            m_bitfield;

  rak::partial_queue  m_downloadCache;

  piece_list_type     m_uploadQueue;
  piece_list_type     m_cancelQueue;

  rak::timer          m_haveTimer;

  Rate                m_peerRate{600};

  ThrottleNode        m_downloadThrottle{30};
  ThrottleNode        m_uploadThrottle{30};
};

}

#endif
