// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_PROTOCOL_INITIAL_SEED_H
#define LIBTORRENT_PROTOCOL_INITIAL_SEED_H

#include "download/download_main.h"

namespace torrent {

class InitialSeeding {
public:
  InitialSeeding(DownloadMain* download);
  ~InitialSeeding();

  static const uint32_t no_offer = ~uint32_t();

  void                new_peer(PeerConnectionBase* pcb);

  // Chunk was seen distributed to a peer in the swarm.
  void                chunk_seen(uint32_t index, PeerConnectionBase* pcb);

  // Returns chunk we may offer the peer or no_offer if none.
  uint32_t            chunk_offer(PeerConnectionBase* pcb, uint32_t indexDone);

  // During the second stage (seeding rare chunks), return
  // false if given chunk is already well-seeded now. True otherwise.
  bool                should_upload(uint32_t index);

private:
  static PeerInfo* const chunk_unsent;  // Chunk never sent to anyone.
  static PeerInfo* const chunk_unknown; // Peer has chunk, we don't know who we sent it to.
  static PeerInfo* const chunk_done;    // Chunk properly distributed by peer.

  uint32_t            find_next(bool secondary, PeerConnectionBase* pcb);

  bool                valid_peer(PeerInfo* peer);
  void                clear_peer(PeerInfo* peer);
  void                chunk_complete(uint32_t index, PeerConnectionBase* pcb);

  void                complete(PeerConnectionBase* pcb);
  void                unblock_all();

  uint32_t            m_nextChunk;
  uint32_t            m_chunksLeft;
  DownloadMain*       m_download;
  PeerInfo**          m_peerChunks;
};

}

#endif
