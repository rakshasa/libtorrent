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

#ifndef LIBTORRENT_HANDSHAKE_H
#define LIBTORRENT_HANDSHAKE_H

#include <rak/priority_queue_default.h>

#include "net/protocol_buffer.h"
#include "net/socket_stream.h"
#include "torrent/bitfield.h"
#include "torrent/peer/peer_info.h"
#include "utils/sha1.h"

#include "handshake_encryption.h"

namespace torrent {

class HandshakeManager;
class DownloadMain;

class Handshake : public SocketStream {
public:
  static const uint32_t part1_size     = 20 + 28;
  static const uint32_t part2_size     = 20;
  static const uint32_t handshake_size = part1_size + part2_size;

  static const uint32_t protocol_bitfield  = 5;
  static const uint32_t protocol_port      = 9;
  static const uint32_t protocol_extension = 20;

  static const uint32_t enc_negotiation_size = 8 + 4 + 2;
  static const uint32_t enc_pad_size         = 512;
  static const uint32_t enc_pad_read_size    = 96 + enc_pad_size + 20;

  static const uint32_t buffer_size = enc_pad_read_size + 20 + enc_negotiation_size + enc_pad_size + 2 + handshake_size;

  typedef ProtocolBuffer<buffer_size> Buffer;

  typedef enum {
    INACTIVE,
    CONNECTING,

    PROXY_CONNECT,
    PROXY_DONE,

    READ_ENC_KEY,
    READ_ENC_SYNC,
    READ_ENC_SKEY,
    READ_ENC_NEGOT,
    READ_ENC_PAD,
    READ_ENC_IA,

    READ_INFO,
    READ_PEER,
    READ_MESSAGE,
    READ_BITFIELD,
    READ_EXT
  } State;

  Handshake(SocketFd fd, HandshakeManager* m, int encryption_options);
  ~Handshake();

  bool                is_active() const             { return m_state != INACTIVE; }

  void                initialize_incoming(const rak::socket_address& sa);
  void                initialize_outgoing(const rak::socket_address& sa, DownloadMain* d, PeerInfo* peerInfo);
  
  PeerInfo*           peer_info()                   { return m_peerInfo; }
  const PeerInfo*     peer_info() const             { return m_peerInfo; }

  void                set_peer_info(PeerInfo* p)    { m_peerInfo = p; }

  const rak::socket_address* socket_address() const { return &m_address; }

  DownloadMain*       download()                    { return m_download; }
  Bitfield*           bitfield()                    { return &m_bitfield; }
  
  void                deactivate_connection();
  void                release_connection();
  void                destroy_connection();

  const void*         unread_data()                 { return m_readBuffer.position(); }
  uint32_t            unread_size() const           { return m_readBuffer.remaining(); }

  rak::timer          initialized_time() const      { return m_initializedTime; }

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

  HandshakeEncryption* encryption()                 { return &m_encryption; }
  ProtocolExtension*   extensions()                  { return m_extensions; }

  int                 retry_options();

protected:
  Handshake(const Handshake&);
  void operator = (const Handshake&);
  
  void                read_done();

  bool                fill_read_buffer(int size);

  // Check what is unnessesary.
  bool                read_proxy_connect();
  bool                read_encryption_key();
  bool                read_encryption_sync();
  bool                read_encryption_skey();
  bool                read_encryption_negotiation();
  bool                read_negotiation_reply();
  bool                read_info();
  bool                read_peer();
  bool                read_bitfield();
  bool                read_extension();

  void                prepare_proxy_connect();
  void                prepare_key_plus_pad();
  void                prepare_enc_negotiation();
  void                prepare_handshake();
  void                prepare_peer_info();
  void                prepare_bitfield();
  void                prepare_keepalive();

  void                write_extension_handshake();
  void                write_bitfield();

  inline void         validate_download();

  uint32_t            read_unthrottled(void* buf, uint32_t length);
  uint32_t            write_unthrottled(const void* buf, uint32_t length);

  static const char*  m_protocol;

  State               m_state;

  HandshakeManager*   m_manager;

  PeerInfo*           m_peerInfo;
  DownloadMain*       m_download;
  Bitfield            m_bitfield;

  ThrottleList*       m_uploadThrottle;
  ThrottleList*       m_downloadThrottle;

  rak::priority_item  m_taskTimeout;
  rak::timer          m_initializedTime;

  uint32_t            m_readPos;
  uint32_t            m_writePos;

  bool                m_readDone;
  bool                m_writeDone;

  bool                m_incoming;

  rak::socket_address m_address;
  char                m_options[8];

  HandshakeEncryption m_encryption;
  ProtocolExtension*  m_extensions;

  // Put these last to keep variables closer to *this.
  Buffer              m_readBuffer;
  Buffer              m_writeBuffer;
};

}

#endif
