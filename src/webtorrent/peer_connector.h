#ifndef LIBTORRENT_WEBTORRENT_PEER_CONNECTOR_H
#define LIBTORRENT_WEBTORRENT_PEER_CONNECTOR_H

#include "config.h"

#ifdef USE_WEBTORRENT

#include <cstdint>
#include <memory>
#include <vector>

#include "torrent/hash_string.h"
#include "webtorrent/data_channel_stream.h"
#include "webtorrent/rtc_signaling.h"

namespace torrent {

class DownloadMain;
class ProtocolExtension;

namespace webtorrent {

class PeerConnector : public std::enable_shared_from_this<PeerConnector> {
public:
  static void connect(DownloadMain* download, RtcStream stream);

  PeerConnector(const PeerConnector&) = delete;
  PeerConnector& operator=(const PeerConnector&) = delete;

private:
  static constexpr const char* protocol_name = "BitTorrent protocol";
  static constexpr uint32_t handshake_size = 68;
  static constexpr uint8_t protocol_unchoke = 1;
  static constexpr uint8_t protocol_have = 4;
  static constexpr uint8_t protocol_bitfield = 5;
  static constexpr uint8_t protocol_extension = 20;

  PeerConnector(DownloadMain* download, RtcStream stream);

  void start();
  void pump();
  void fail();
  bool flush_write_buffer();
  bool read_available();
  bool try_read_handshake();
  void queue_initial_messages();
  void promote();

  void queue_bytes(const void* data, size_t size);
  void queue_uint32(uint32_t value);

  DownloadMain* m_download{};
  std::unique_ptr<DataChannelStream> m_stream;
  std::shared_ptr<PeerConnector> m_self;

  std::vector<char> m_read_buffer;
  std::vector<char> m_write_buffer;
  size_t m_write_position{};

  HashString m_peer_id{HashString::new_zero()};
  char m_options[8]{};
  bool m_handshake_read{false};
  bool m_initial_messages_queued{false};
  bool m_promoted{false};
};

} // namespace webtorrent

} // namespace torrent

#endif // USE_WEBTORRENT

#endif // LIBTORRENT_WEBTORRENT_PEER_CONNECTOR_H
