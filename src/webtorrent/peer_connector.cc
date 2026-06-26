#include "config.h"

#ifdef USE_WEBTORRENT

#include "webtorrent/peer_connector.h"

#include <algorithm>
#include <cstring>

#include "download/download_main.h"
#include "protocol/encryption_info.h"
#include "protocol/extensions.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_transport.h"
#include "torrent/bitfield.h"
#include "torrent/data/file_list.h"
#include "torrent/download_info.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/peer_list.h"
#include "thread_main.h"

namespace torrent::webtorrent {

void
PeerConnector::connect(DownloadMain* download, RtcStream stream) {
  auto connector = std::shared_ptr<PeerConnector>(new PeerConnector(download, std::move(stream)));
  connector->m_self = connector;
  connector->start();
}

PeerConnector::PeerConnector(DownloadMain* download, RtcStream stream)
  : m_download(download),
    m_stream(std::make_unique<DataChannelStream>(std::move(stream))) {
}

void
PeerConnector::start() {
  std::weak_ptr<PeerConnector> weak_self = shared_from_this();

  m_stream->slot_readable([weak_self] {
      main_thread::callback([weak_self] {
          if (auto self = weak_self.lock())
            self->pump();
        });
    });

  m_stream->slot_writable([weak_self] {
      main_thread::callback([weak_self] {
          if (auto self = weak_self.lock())
            self->pump();
        });
    });

  m_stream->slot_closed([weak_self] {
      main_thread::callback([weak_self] {
          if (auto self = weak_self.lock())
            self->fail();
        });
    });

  char header[handshake_size]{};
  header[0] = 19;
  std::memcpy(header + 1, protocol_name, 19);
  header[25] |= 0x10;
  std::memcpy(header + 28, m_download->info()->hash().c_str(), 20);
  std::memcpy(header + 48, m_download->info()->local_id().c_str(), 20);
  queue_bytes(header, sizeof(header));

  pump();
}

void
PeerConnector::pump() {
  if (m_promoted || !m_stream || !m_stream->is_open())
    return;

  flush_write_buffer();
  read_available();

  if (!m_handshake_read && !try_read_handshake())
    return;

  if (!m_initial_messages_queued) {
    queue_initial_messages();
    m_initial_messages_queued = true;
  }

  if (flush_write_buffer())
    promote();
}

void
PeerConnector::fail() {
  if (m_promoted)
    return;

  if (m_stream)
    m_stream->close();

  m_self.reset();
}

bool
PeerConnector::flush_write_buffer() {
  while (m_write_position < m_write_buffer.size()) {
    int written = m_stream->write_stream(m_write_buffer.data() + m_write_position,
                                         m_write_buffer.size() - m_write_position);

    if (written <= 0)
      return false;

    m_write_position += written;
  }

  m_write_buffer.clear();
  m_write_position = 0;
  return true;
}

bool
PeerConnector::read_available() {
  while (m_stream->available() != 0) {
    char buffer[4096];
    int length = m_stream->read_stream(buffer, std::min<size_t>(sizeof(buffer), m_stream->available()));

    if (length <= 0)
      return false;

    m_read_buffer.insert(m_read_buffer.end(), buffer, buffer + length);
  }

  return true;
}

bool
PeerConnector::try_read_handshake() {
  if (m_read_buffer.size() < handshake_size)
    return false;

  if (static_cast<uint8_t>(m_read_buffer[0]) != 19 ||
      std::memcmp(m_read_buffer.data() + 1, protocol_name, 19) != 0 ||
      m_download->info()->hash().not_equal_to(m_read_buffer.data() + 28)) {
    fail();
    return false;
  }

  if (std::memcmp(m_read_buffer.data() + 48, m_download->info()->local_id().c_str(), 20) == 0) {
    fail();
    return false;
  }

  std::memcpy(m_options, m_read_buffer.data() + 20, sizeof(m_options));
  m_peer_id.assign(m_read_buffer.data() + 48);
  m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + handshake_size);
  m_handshake_read = true;
  return true;
}

void
PeerConnector::queue_initial_messages() {
  if (m_options[5] & 0x10) {
    ProtocolExtension extensions;
    extensions.set_info(nullptr, m_download);
    DataBuffer message = extensions.generate_handshake_message();

    queue_uint32(message.length() + 2);
    uint8_t message_id = protocol_extension;
    uint8_t extended_id = ProtocolExtension::HANDSHAKE;
    queue_bytes(&message_id, sizeof(message_id));
    queue_bytes(&extended_id, sizeof(extended_id));
    queue_bytes(message.data(), message.length());
  }

  auto bitfield = m_download->file_list()->bitfield();

  if (bitfield->is_all_unset()) {
    queue_uint32(0);
    return;
  }

  queue_uint32(bitfield->size_bytes() + 1);
  uint8_t message_id = protocol_bitfield;
  queue_bytes(&message_id, sizeof(message_id));
  queue_bytes(bitfield->begin(), bitfield->size_bytes());

  // Browser WebTorrent clients may receive the bitfield before metadata is
  // fully initialized. Replay availability as HAVE messages so the peer marks
  // us interesting once metadata arrives.
  for (uint32_t index = 0; index < bitfield->size_bits(); ++index) {
    if (!bitfield->get(index))
      continue;

    queue_uint32(5);
    message_id = protocol_have;
    queue_bytes(&message_id, sizeof(message_id));
    queue_uint32(index);
  }

  queue_uint32(1);
  message_id = protocol_unchoke;
  queue_bytes(&message_id, sizeof(message_id));
}

void
PeerConnector::promote() {
  if (m_promoted)
    return;

  auto peer_info = m_download->peer_list()->connected_webtorrent(m_peer_id);

  if (peer_info == nullptr) {
    fail();
    return;
  }

  std::memcpy(peer_info->set_options(), m_options, sizeof(m_options));

  Bitfield bitfield;
  bitfield.set_size_bits(m_download->file_list()->bitfield()->size_bits());
  bitfield.allocate();
  bitfield.unset_all();

  EncryptionInfo encryption_info;
  ProtocolExtension* extensions = (m_options[5] & 0x10) ? new ProtocolExtension : HandshakeManager::default_extensions();

  if (!extensions->is_default())
    extensions->set_info(peer_info, m_download);

  auto connection = m_download->connection_list()->insert_transport(peer_info, PeerTransport::create_webtorrent(std::move(m_stream)),
                                                                    &bitfield, &encryption_info, extensions);

  if (connection == nullptr) {
    if (!extensions->is_default())
      delete extensions;

    m_download->peer_list()->disconnected(peer_info, 0);
    fail();
    return;
  }

  m_promoted = true;

  if (!m_read_buffer.empty()) {
    connection->push_unread(m_read_buffer.data(), m_read_buffer.size());
    connection->read_insert_poll_safe();
  }

  if (peer_info->connection() == connection)
    connection->write_insert_poll_safe();

  m_self.reset();
}

void
PeerConnector::queue_bytes(const void* data, size_t size) {
  auto first = static_cast<const char*>(data);
  m_write_buffer.insert(m_write_buffer.end(), first, first + size);
}

void
PeerConnector::queue_uint32(uint32_t value) {
  char buffer[4] = {
    static_cast<char>((value >> 24) & 0xff),
    static_cast<char>((value >> 16) & 0xff),
    static_cast<char>((value >> 8) & 0xff),
    static_cast<char>(value & 0xff),
  };

  queue_bytes(buffer, sizeof(buffer));
}

} // namespace torrent::webtorrent

#endif // USE_WEBTORRENT
