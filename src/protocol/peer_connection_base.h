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

#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H

#include "data/chunk.h"
#include "data/chunk_handle.h"
#include "data/piece.h"
#include "net/manager.h"
#include "net/socket_stream.h"
#include "utils/bitfield_ext.h"
#include "utils/task.h"
#include "utils/throttle.h"
#include "torrent/rate.h"

#include "peer_info.h"
#include "protocol_base.h"
#include "request_list.h"

namespace torrent {

// Base class for peer connection classes. Rename to PeerConnection
// when the migration is complete.

class DownloadMain;

class PeerConnectionBase : public SocketStream {
public:
  typedef std::list<Piece>    PieceList;
  typedef std::list<uint32_t> HaveQueue;
  typedef ProtocolBase        ProtocolRead;
  typedef ProtocolBase        ProtocolWrite;

  // Find an optimal number for this.
  static const uint32_t read_size = 64;

  PeerConnectionBase();
  virtual ~PeerConnectionBase();
  
  void                initialize(DownloadMain* download, const PeerInfo& p, SocketFd fd);

  bool                is_up_choked()                { return m_up->choked(); }
  bool                is_up_interested()            { return m_up->interested(); }
  bool                is_down_choked()              { return m_down->choked(); }
  bool                is_down_interested()          { return m_down->interested(); }

  bool                is_upload_wanted() const      { return m_down->interested() && !m_snubbed; }

  bool                is_down_throttled()           { return m_downThrottle != throttleRead.end(); }
  bool                is_up_throttled()             { return m_upThrottle != throttleWrite.end(); }

  bool                is_snubbed() const            { return m_snubbed; }
  bool                is_seeder() const             { return m_bitfield.all_set(); }

  const PeerInfo&     get_peer() const              { return m_peer; }

  Rate&               peer_rate()                   { return m_peerRate; }
  Rate&               up_rate()                     { return m_upRate; }
  Rate&               down_rate()                   { return m_downRate; }

  RequestList&        get_request_list()            { return m_requestList; }
  PieceList&          get_send_list()               { return m_sendList; }

  const BitFieldExt&  bitfield() const          { return m_bitfield; }

  // Make sure you choke the peer when snubbing. Snubbing a peer will
  // only cause it not to be unchoked.
  void                set_snubbed(bool v);

  Timer               time_last_choked() const      { return m_timeLastChoked; }

  void                insert_down_throttle();
  void                remove_down_throttle();

  void                insert_up_throttle();
  void                remove_up_throttle();

  // These must be implemented by the child class.
  virtual void        initialize_custom() = 0;

  virtual void        update_interested() = 0;

  virtual void        receive_finished_chunk(int32_t i) = 0;
  virtual bool        receive_keepalive() = 0;
  void                receive_choke(bool v);

  virtual void        event_error();

protected:
  typedef Chunk::iterator ChunkPart;

  inline bool         read_remaining();
  inline bool         write_remaining();

  void                load_down_chunk(const Piece& p);
  void                load_up_chunk();

  void                receive_throttle_down_activate();
  void                receive_throttle_up_activate();

  void                read_request_piece(const Piece& p);
  void                read_cancel_piece(const Piece& p);

  void                read_buffer_move_unused();

  void                write_prepare_piece();
  void                write_finished_piece();

  bool                read_bitfield_body();
  bool                read_bitfield_from_buffer(uint32_t msgLength);

  bool                write_bitfield_body();

  bool                down_chunk();
  bool                down_chunk_from_buffer();
  bool                up_chunk();

  void                down_chunk_release();
  void                up_chunk_release();

  bool                should_request();
  bool                try_request_pieces();

  void                set_remote_interested();
  void                set_remote_not_interested();

  DownloadMain*       m_download;

  ProtocolRead*       m_down;
  ProtocolWrite*      m_up;

  PeerInfo            m_peer;
  Rate                m_peerRate;

  Rate                m_downRate;
  ThrottlePeerNode    m_downThrottle;
  Piece               m_downPiece;
  ChunkHandle         m_downChunk;

  uint32_t            m_downStall;

  Rate                m_upRate;
  ThrottlePeerNode    m_upThrottle;
  Piece               m_upPiece;
  ChunkHandle         m_upChunk;

  RequestList         m_requestList;
  HaveQueue           m_haveQueue;

  PieceList           m_sendList;
  bool                m_sendChoked;
  bool                m_sendInterested;

  bool                m_snubbed;
  BitFieldExt         m_bitfield;
  Timer               m_timeLastChoked;

  Timer               m_timeLastRead;
};

inline void
PeerConnectionBase::insert_down_throttle() {
  if (!is_down_throttled())
    m_downThrottle = throttleRead.insert(PeerConnectionThrottle(this, &PeerConnectionBase::receive_throttle_down_activate));
}

inline void
PeerConnectionBase::remove_down_throttle() {
  if (is_down_throttled()) {
    throttleRead.erase(m_downThrottle);
    m_downThrottle = throttleRead.end();
  }
}

inline void
PeerConnectionBase::insert_up_throttle() {
  if (!is_up_throttled())
    m_upThrottle = throttleWrite.insert(PeerConnectionThrottle(this, &PeerConnectionBase::receive_throttle_up_activate));
}

inline void
PeerConnectionBase::remove_up_throttle() {
  if (is_up_throttled()) {
    throttleWrite.erase(m_upThrottle);
    m_upThrottle = throttleWrite.end();
  }
}

inline bool
PeerConnectionBase::read_remaining() {
  m_down->buffer()->move_position(read_stream_throws(m_down->buffer()->position(),
						     m_down->buffer()->remaining()));

  return !m_down->buffer()->remaining();
}

inline bool
PeerConnectionBase::write_remaining() {
  m_up->buffer()->move_position(write_stream_throws(m_up->buffer()->position(),
						m_up->buffer()->remaining()));

  return !m_up->buffer()->remaining();
}

}

#endif
