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

#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H

#include "data/chunk_handle.h"
#include "download/choke_manager_node.h"
#include "net/socket_stream.h"
#include "torrent/poll.h"

#include "encryption_info.h"
#include "extensions.h"
#include "peer_chunks.h"
#include "protocol_base.h"
#include "request_list.h"

#include "globals.h"

#include "manager.h"

namespace torrent {

// Base class for peer connection classes. Rename to PeerConnection
// when the migration is complete?
//
// This should really be modularized abit, there's too much stuff in
// PeerConnectionBase and its children. Do we use additional layers of
// inheritance or member instances?

class DownloadMain;

class PeerConnectionBase : public SocketStream {
public:
  typedef ProtocolBase           ProtocolRead;
  typedef ProtocolBase           ProtocolWrite;

#if USE_EXTRA_DEBUG == 666
  // For testing, use a really small buffer.
  typedef ProtocolBuffer<256>  EncryptBuffer;
#else
  typedef ProtocolBuffer<16384>  EncryptBuffer;
#endif

  // Find an optimal number for this.
  static const uint32_t read_size = 64;

  // Bitmasks for peer exchange messages to send.
  static const int PEX_DO      = (1 << 0);
  static const int PEX_ENABLE  = (1 << 1);
  static const int PEX_DISABLE = (1 << 2);

  PeerConnectionBase();
  virtual ~PeerConnectionBase();
  
  void                initialize(DownloadMain* download, PeerInfo* p, SocketFd fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions);
  void                cleanup();

  bool                is_up_choked() const            { return m_upChoke.choked(); }
  bool                is_up_interested() const        { return m_upChoke.queued(); }
  bool                is_up_snubbed() const           { return m_upChoke.snubbed(); }

  bool                is_down_queued() const          { return m_downChoke.queued(); }
  bool                is_down_local_unchoked() const  { return m_downChoke.unchoked(); }
  bool                is_down_remote_unchoked() const { return m_downUnchoked; }
  bool                is_down_interested() const      { return m_downInterested; }

  void                set_upload_snubbed(bool v);

  bool                is_seeder() const               { return m_peerChunks.is_seeder(); }
  bool                is_not_seeder() const           { return !m_peerChunks.is_seeder(); }

  bool                is_encrypted() const            { return m_encryption.is_encrypted(); }
  bool                is_obfuscated() const           { return m_encryption.is_obfuscated(); }

  PeerInfo*           peer_info()                     { return m_peerInfo; }
  const PeerInfo*     c_peer_info() const             { return m_peerInfo; }

  PeerChunks*         peer_chunks()                   { return &m_peerChunks; }
  const PeerChunks*   c_peer_chunks() const           { return &m_peerChunks; }

  DownloadMain*       download()                      { return m_download; }
  RequestList*        download_queue()                { return &m_downloadQueue; }
  const RequestList*  download_queue() const          { return &m_downloadQueue; }

  ProtocolExtension*  extensions()                    { return m_extensions; }
  DataBuffer*         extension_message()             { return &m_extensionMessage; }

  void                do_peer_exchange()              { m_sendPEXMask |= PEX_DO; }
  inline void         set_peer_exchange(bool state);
  
  // These must be implemented by the child class.
  virtual void        initialize_custom() = 0;
  virtual void        update_interested() = 0;
  virtual bool        receive_keepalive() = 0;

  bool                receive_upload_choke(bool choke);
  bool                receive_download_choke(bool choke);

  virtual void        event_error();

  void                push_unread(const void* data, uint32_t size);

  void                cancel_transfer(BlockTransfer* transfer);

protected:
  static const uint32_t extension_must_encrypt = ~uint32_t();

  inline bool         read_remaining();
  inline bool         write_remaining();

  void                load_up_chunk();

  void                read_request_piece(const Piece& p);
  void                read_cancel_piece(const Piece& p);

  void                write_prepare_piece();
  void                write_prepare_extension(int type, const DataBuffer& message);

  bool                down_chunk_start(const Piece& p);
  void                down_chunk_finished();

  bool                down_chunk();
  bool                down_chunk_from_buffer();
  bool                down_chunk_skip();
  bool                down_chunk_skip_from_buffer();

  uint32_t            down_chunk_process(const void* buffer, uint32_t length);
  uint32_t            down_chunk_skip_process(const void* buffer, uint32_t length);

  bool                down_extension();

  bool                up_chunk();
  inline uint32_t     up_chunk_encrypt(uint32_t quota);

  bool                up_extension();

  void                down_chunk_release();
  void                up_chunk_release();

  bool                should_request();
  bool                try_request_pieces();

  void                send_pex_message();

  // Insert into the poll unless we're blocking for throttling etc.
  void                read_insert_poll_safe();
  void                write_insert_poll_safe();

  DownloadMain*       m_download;

  ProtocolRead*       m_down;
  ProtocolWrite*      m_up;

  PeerInfo*           m_peerInfo;
  PeerChunks          m_peerChunks;

  RequestList         m_downloadQueue;
  ChunkHandle         m_downChunk;
  uint32_t            m_downStall;

  Piece               m_upPiece;
  ChunkHandle         m_upChunk;

  // The interested state no longer follows the spec's wording as it
  // has been swapped.
  //
  // Thus the same ProtocolBase object now groups the same choke and
  // interested states togheter, thus for m_up 'interested' means the
  // remote peer wants upload and 'choke' means we've choked upload to
  // that peer.
  //
  // In the downlod object, 'queued' now means the same as the spec's
  // 'unchoked', while 'unchoked' means we start requesting pieces.
  ChokeManagerNode    m_upChoke;
  ChokeManagerNode    m_downChoke;

  bool                m_downInterested;
  bool                m_downUnchoked;

  bool                m_sendChoked;
  bool                m_sendInterested;
  bool                m_tryRequest;

  int                 m_sendPEXMask;

  rak::timer          m_timeLastRead;

  DataBuffer          m_extensionMessage;
  uint32_t            m_extensionOffset;

  EncryptBuffer*      m_encryptBuffer;
  EncryptionInfo      m_encryption;
  ProtocolExtension*  m_extensions;
};

inline void
PeerConnectionBase::set_peer_exchange(bool state) {
  if (m_extensions->is_default() || !m_extensions->is_remote_supported(ProtocolExtension::UT_PEX))
    return;

  if (state) {
    m_sendPEXMask = PEX_ENABLE | (m_sendPEXMask & ~PEX_DISABLE);
    m_extensions->set_local_enabled(ProtocolExtension::UT_PEX);
  } else {
    m_sendPEXMask = PEX_DISABLE | (m_sendPEXMask & ~PEX_ENABLE);
    m_extensions->unset_local_enabled(ProtocolExtension::UT_PEX);
  }
}

inline void
PeerConnectionBase::push_unread(const void* data, uint32_t size) {
  std::memcpy(m_down->buffer()->end(), data, size);
  m_down->buffer()->move_end(size);
}

inline void
PeerConnectionBase::read_insert_poll_safe() {
  if (m_down->get_state() != ProtocolRead::IDLE)
    return;

  manager->poll()->insert_read(this);
}

inline void
PeerConnectionBase::write_insert_poll_safe() {
  if (m_up->get_state() != ProtocolWrite::IDLE)
    return;

  manager->poll()->insert_write(this);
}

}

#endif
