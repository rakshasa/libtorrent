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

#ifndef LIBTORRENT_NET_PEER_CONNECTION_BASE_H
#define LIBTORRENT_NEW_PEER_CONNECTION_BASE_H

#include "data/chunk.h"
#include "data/piece.h"
#include "net/manager.h"
#include "net/socket_stream.h"
#include "utils/bitfield_ext.h"
#include "utils/task.h"
#include "utils/throttle.h"
#include "torrent/rate.h"

#include "peer_info.h"
#include "protocol_read.h"
#include "protocol_write.h"
#include "request_list.h"

namespace torrent {

// Base class for peer connection classes. Rename to PeerConnection
// when the migration is complete.

class ChunkListNode;
class DownloadMain;

class PeerConnectionBase : public SocketStream {
public:
  typedef std::list<Piece> PieceList;

  PeerConnectionBase();
  virtual ~PeerConnectionBase();
  
  bool                is_write_choked()             { return m_write->get_choked(); }
  bool                is_write_interested()         { return m_write->get_interested(); }
  bool                is_read_choked()              { return m_read->get_choked(); }
  bool                is_read_interested()          { return m_read->get_interested(); }

  bool                is_read_throttled()           { return m_readThrottle != throttleRead.end(); }
  bool                is_write_throttled()          { return m_writeThrottle != throttleWrite.end(); }

  bool                is_snubbed() const            { return m_snubbed; }

  const PeerInfo&     get_peer() const              { return m_peer; }

  Rate&               get_peer_rate()               { return m_peerRate; }
  Rate&               get_write_rate()              { return m_writeRate; }
  Rate&               get_read_rate()               { return m_readRate; }

  Timer               get_last_choked()             { return m_lastChoked; }

  RequestList&        get_request_list()            { return m_requestList; }
  PieceList&          get_send_list()               { return m_sendList; }

  const BitFieldExt&  get_bitfield() const          { return m_bitfield; }

  // Make sure you choke the peer when snubbing. Snubbing a peer will
  // only cause it not to be unchoked.
  void                set_snubbed(bool v)           { m_snubbed = v; }
  virtual void        set_choke(bool v) = 0;

  uint32_t            pipe_size() const;
  bool                should_request();

  void                insert_read_throttle();
  void                remove_read_throttle();

  void                insert_write_throttle();
  void                remove_write_throttle();

  virtual void        update_interested() = 0;

  virtual void        receive_have_chunk(int32_t i) = 0;
  virtual bool        receive_keepalive() = 0;

protected:
  inline bool         read_remaining();
  inline bool         write_remaining();

  bool                write_chunk();
  bool                read_chunk();

  void                load_read_chunk(const Piece& p);

  void                receive_throttle_read_activate();
  void                receive_throttle_write_activate();

  DownloadMain*       m_download;

  ProtocolRead*       m_read;
  ProtocolWrite*      m_write;

  PeerInfo            m_peer;
  Rate                m_peerRate;

  Rate                m_readRate;
  ThrottlePeerNode    m_readThrottle;
  uint32_t            m_readStall;

  Rate                m_writeRate;
  ThrottlePeerNode    m_writeThrottle;

  RequestList         m_requestList;
  PieceList           m_sendList;

  bool                m_snubbed;
  BitFieldExt         m_bitfield;
  Timer               m_lastChoked;

  typedef Chunk::iterator ChunkPart;

  // Read chunk de-abstracting.
  uint32_t            down_chunk(uint32_t maxBytes);
  inline bool         down_chunk_part(ChunkPart c, uint32_t& left);

  uint32_t            m_downChunkPosition;
  Piece               m_downPiece;
  ChunkListNode*      m_downChunk;

  // Write chunk de-abstracting.
  uint32_t            up_chunk(uint32_t maxBytes);
  inline bool         up_chunk_part(ChunkPart c, uint32_t& left);

  uint32_t            m_upChunkPosition;
  Piece               m_upPiece;
  ChunkListNode*      m_upChunk;
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
  m_read->get_buffer().move_position(read_buf(m_read->get_buffer().position(),
					      m_read->get_buffer().remaining()));

  return !m_read->get_buffer().remaining();
}

inline bool
PeerConnectionBase::write_remaining() {
  m_write->get_buffer().move_position(write_buf(m_write->get_buffer().position(),
						m_write->get_buffer().remaining()));

  return !m_write->get_buffer().remaining();
}

}

#endif
