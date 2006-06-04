// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "net/throttle_node.h"
#include "torrent/bitfield.h"
#include "torrent/piece.h"
#include "torrent/rate.h"

namespace torrent {

class PeerInfo;

class PeerChunks {
public:
  typedef std::list<uint32_t> index_list_type;
  typedef std::list<Piece>    piece_list_type;

  PeerChunks();

  bool                is_seeder() const             { return m_bitfield.is_all_set(); }

  bool                is_snubbed() const            { return m_snubbed; }
  void                set_snubbed(bool v)           { m_snubbed = v; }

  PeerInfo*           peer_info()                   { return m_peerInfo; }
  const PeerInfo*     peer_info() const             { return m_peerInfo; }
  void                set_peer_info(PeerInfo* p)    { m_peerInfo = p; }

  bool                using_counter() const         { return m_usingCounter; }
  void                set_using_counter(bool state) { m_usingCounter = state; }

  Bitfield*           bitfield()                    { return &m_bitfield; }
  const Bitfield*     bitfield() const              { return &m_bitfield; }

  rak::partial_queue* download_cache()              { return &m_downloadCache; }
  //RequestList*        download_queue()              { return &m_downloadQueue; }

  piece_list_type*    upload_queue()                { return &m_uploadQueue; }
  index_list_type*    have_queue()                  { return &m_haveQueue; }

  Rate*               peer_rate()                   { return &m_peerRate; }

  ThrottleNode*       download_throttle()           { return &m_downloadThrottle; }
  ThrottleNode*       upload_throttle()             { return &m_uploadThrottle; }

private:
  PeerInfo*           m_peerInfo;

  bool                m_snubbed;
  bool                m_usingCounter;

  Bitfield            m_bitfield;

  rak::partial_queue  m_downloadCache;

  piece_list_type     m_uploadQueue;
  index_list_type     m_haveQueue;

  Rate                m_peerRate;

  ThrottleNode        m_downloadThrottle;
  ThrottleNode        m_uploadThrottle;
};

inline
PeerChunks::PeerChunks() :
  m_peerInfo(NULL),

  m_snubbed(false),
  m_usingCounter(false),

  m_peerRate(600),

  m_downloadThrottle(30),
  m_uploadThrottle(30)
{
}

}

#endif
