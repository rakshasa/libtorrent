#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_METADATA_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_METADATA_H

#include "peer_connection_base.h"

#include "torrent/download.h"

namespace torrent {

class PeerConnectionMetadata : public PeerConnectionBase {
public:
  ~PeerConnectionMetadata() override;

  void                initialize_custom() override;
  void                update_interested() override;
  bool                receive_keepalive() override;

  void                event_read() override;
  void                event_write() override;

  void                receive_metadata_piece(uint32_t piece, const char* data, uint32_t length) override;

private:
  inline bool         read_message();

  bool                read_skip_bitfield();

  bool                try_request_metadata_pieces();

  inline void         fill_write_buffer();

  uint32_t            m_skipLength;
};

} // namespace torrent

#endif
