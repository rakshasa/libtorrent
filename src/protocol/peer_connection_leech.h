#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_LEECH_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_LEECH_H

#include "peer_connection_base.h"

#include "torrent/download.h"

namespace torrent {

// Type-specific data.
template<Download::ConnectionType type> struct PeerConnectionData;

template<> struct PeerConnectionData<Download::CONNECTION_LEECH> { };

template<> struct PeerConnectionData<Download::CONNECTION_SEED> { };

template<> struct PeerConnectionData<Download::CONNECTION_INITIAL_SEED> {
  uint32_t lastIndex{~uint32_t{}};
  uint32_t bytesLeft;
};

template<Download::ConnectionType type>
class PeerConnection : public PeerConnectionBase {
public:
  PeerConnection() = default;
  ~PeerConnection() override;

  void                initialize_custom() override;
  void                update_interested() override;
  bool                receive_keepalive() override;

  void                event_read() override;
  void                event_write() override;

private:
  PeerConnection(const PeerConnection&) = delete;
  PeerConnection& operator=(const PeerConnection&) = delete;

  inline bool         read_message();
  void                read_have_chunk(uint32_t index);

  void                offer_chunk();
  bool                should_upload();

  inline void         fill_write_buffer();

  PeerConnectionData<type> m_data;
};

} // namespace torrent

#endif
