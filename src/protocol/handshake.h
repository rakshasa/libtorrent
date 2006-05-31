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

#ifndef LIBTORRENT_HANDSHAKE_H
#define LIBTORRENT_HANDSHAKE_H

#include <rak/priority_queue_default.h>

#include "net/protocol_buffer.h"
#include "net/socket_stream.h"
#include "torrent/bitfield.h"

#include "peer_info.h"

namespace torrent {

class HandshakeManager;
class DownloadMain;

class Handshake : public SocketStream {
public:
  static const uint32_t part1_size     = 20 + 28;
  static const uint32_t part2_size     = 20;
  static const uint32_t handshake_size = part1_size + part2_size;

  static const uint32_t protocol_bitfield = 5;

  typedef ProtocolBuffer<handshake_size> Buffer;

  typedef enum {
    INACTIVE,
    CONNECTING,
    WRITE_FILL,
    WRITE_SEND,
    READ_INFO,
    READ_PEER,

    BITFIELD
  } State;

  Handshake(SocketFd fd, HandshakeManager* m);
  ~Handshake();

  void                initialize_outgoing(const rak::socket_address& sa, DownloadMain* d);
  void                initialize_incoming(const rak::socket_address& sa);
  
  PeerInfo*           peer_info()                   { return m_peerInfo; }
  const PeerInfo*     peer_info() const             { return m_peerInfo; }

  void                set_peer_info(PeerInfo* p)    { m_peerInfo = p; }

  const rak::socket_address* socket_address() const       { return &m_address; }

  DownloadMain*       download()                    { return m_download; }
  Bitfield*           bitfield()                    { return &m_bitfield; }
  
  // Make sure the fd is valid when this is called. The caller is
  // responsible for closing the socket if nessesary.
  void                clear();

  const void*         unread_data()                 { return m_readBuffer.position(); }
  uint32_t            unread_size() const           { return m_readBuffer.remaining(); }

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

protected:
  Handshake(const Handshake&);
  void operator = (const Handshake&);
  
  void                read_done();

  inline void         prepare_peer_info();

  inline void         prepare_write_bitfield();
  inline void         prepare_write_keepalive();

  static const char*  m_protocol;

  State               m_state;

  HandshakeManager*   m_manager;

  PeerInfo*           m_peerInfo;
  DownloadMain*       m_download;
  Bitfield            m_bitfield;

  rak::priority_item  m_taskTimeout;

  uint32_t            m_readPos;
  Buffer              m_readBuffer;
  bool                m_readDone;

  uint32_t            m_writePos;
  Buffer              m_writeBuffer;
  bool                m_writeDone;

  bool                m_incoming;

  rak::socket_address m_address;
  char                m_options[8];
};

}

#endif
