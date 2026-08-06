#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_BASE_H

#include "data/chunk_handle.h"
#include "net/data_buffer.h"
#include "net/socket_stream.h"
#include "protocol/encryption_info.h"
#include "protocol/peer_chunks.h"
#include "protocol/protocol_base.h"
#include "protocol/request_list.h"
#include "torrent/system/poll.h"
#include "torrent/peer/choke_status.h"
#include "torrent/peer/peer.h"

namespace torrent {

// Base class for peer connection classes. Rename to PeerConnection
// when the migration is complete?
//
// This should really be modularized abit, there's too much stuff in
// PeerConnectionBase and its children. Do we use additional layers of
// inheritance or member instances?

class choke_queue;
class DownloadMain;

class PeerConnectionBase : public Peer, public SocketStream {
public:
  using ProtocolRead  = ProtocolBase;
  using ProtocolWrite = ProtocolBase;

#if USE_EXTRA_DEBUG == 666
  // For testing, use a really small buffer.
  typedef ProtocolBuffer<256>  EncryptBuffer;
#else
  using EncryptBuffer = ProtocolBuffer<16384>;
#endif

  // Bitmasks for peer exchange messages to send.
  static constexpr int PEX_DO      = (1 << 0);
  static constexpr int PEX_ENABLE  = (1 << 1);
  static constexpr int PEX_DISABLE = (1 << 2);

  PeerConnectionBase();
  ~PeerConnectionBase() override;

  const char*         type_name() const override      { return "pcb"; }

  void                initialize(DownloadMain* download, PeerInfo* p, int fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions);
  void                cleanup();

  bool                is_up_choked() const            { return m_up_choke.choked(); }
  bool                is_up_interested() const        { return m_up_choke.queued(); }
  bool                is_up_snubbed() const           { return m_up_choke.snubbed(); }

  bool                is_down_queued() const          { return m_down_choke.queued(); }
  bool                is_down_local_unchoked() const  { return m_down_choke.unchoked(); }
  bool                is_down_remote_unchoked() const { return m_down_unchoked; }
  bool                is_down_interested() const      { return m_down_interested; }

  void                set_upload_snubbed(bool v);

  bool                is_seeder() const               { return m_peer_chunks.is_seeder(); }
  bool                is_not_seeder() const           { return !m_peer_chunks.is_seeder(); }

  bool                is_encrypted() const            { return m_encryption.is_encrypted(); }
  bool                is_obfuscated() const           { return m_encryption.is_obfuscated(); }

  PeerInfo*           mutable_peer_info()             { return m_peerInfo; }

  PeerChunks*         peer_chunks()                   { return &m_peer_chunks; }
  const PeerChunks*   c_peer_chunks() const           { return &m_peer_chunks; }

  choke_status*       up_choke()                      { return &m_up_choke; }
  choke_status*       down_choke()                    { return &m_down_choke; }

  DownloadMain*       download()                      { return m_download; }
  RequestList*        request_list()                  { return &m_request_list; }
  const RequestList*  request_list() const            { return &m_request_list; }

  ProtocolExtension*  extensions()                    { return m_extensions; }
  DataBuffer*         extension_message()             { return &m_extension_message; }

  void                do_peer_exchange()              { m_send_pex_mask |= PEX_DO; }
  void                set_peer_exchange(bool state);

  // These must be implemented by the child class.
  virtual void        initialize_custom() = 0;
  virtual void        update_interested() = 0;
  virtual bool        receive_keepalive() = 0;

  bool                receive_upload_choke(bool choke);
  bool                receive_download_choke(bool choke);

  void                event_error() override;

  void                push_unread(const void* data, uint32_t size);

  void                cancel_transfer(BlockTransfer* transfer);

  // Insert into the poll unless we're blocking for throttling etc.
  void                read_insert_poll_safe();
  void                write_insert_poll_safe();

  // Communication with the protocol extensions
  virtual void        receive_metadata_piece(uint32_t piece, const char* data, uint32_t length);

  bool                should_connection_unchoke(choke_queue* cq) const;

protected:
  static constexpr uint32_t extension_must_encrypt = ~uint32_t();

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

  bool                send_pex_message();
  bool                send_ext_message();

  DownloadMain*       m_download{};

  ProtocolRead*       m_down;
  ProtocolWrite*      m_up;

  PeerChunks          m_peer_chunks;

  RequestList         m_request_list;
  ChunkHandle         m_down_chunk;
  uint32_t            m_down_stall{};

  Piece               m_up_piece;
  ChunkHandle         m_up_chunk;

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
  choke_status        m_up_choke;
  choke_status        m_down_choke;

  bool                m_down_interested{};
  bool                m_down_unchoked{};

  bool                m_send_choked{};
  bool                m_send_interested{};
  bool                m_tryRequest{};

  int                 m_send_pex_mask{};

  std::chrono::microseconds m_time_last_read{};

  DataBuffer          m_extension_message;
  uint32_t            m_extension_offset;

  std::unique_ptr<EncryptBuffer> m_encrypt_buffer;

  EncryptionInfo      m_encryption;
  ProtocolExtension*  m_extensions{};

  bool                m_incore_continous{};
};

inline void
PeerConnectionBase::push_unread(const void* data, uint32_t size) {
  std::memcpy(m_down->buffer()->end(), data, size);
  m_down->buffer()->move_end(size);
}

inline void
PeerConnectionBase::read_insert_poll_safe() {
  if (m_down->get_state() != ProtocolRead::IDLE)
    return;

  this_thread::poll()->insert_read(this);
}

inline void
PeerConnectionBase::write_insert_poll_safe() {
  if (m_up->get_state() != ProtocolWrite::IDLE)
    return;

  this_thread::poll()->insert_write(this);
}

} // namespace torrent

#endif
