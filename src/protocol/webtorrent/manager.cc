#include "config.h"

#include "protocol/webtorrent/manager.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <sstream>
#include <utility>

#ifdef USE_WEBTORRENT
#include "data/chunk.h"
#include "data/chunk_handle.h"
#include "data/chunk_iterator.h"
#include "data/chunk_list.h"
#include "download/chunk_selector.h"
#include "download/download_main.h"
#include "download/download_wrapper.h"
#include "manager.h"
#include "net/thread_net.h"
#include "protocol/extensions.h"
#include "protocol/protocol_base.h"
#include "protocol/webtorrent/data_channel_stream.h"
#include "torrent/bitfield.h"
#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/data/block_transfer.h"
#include "torrent/data/file_list.h"
#include "torrent/data/piece.h"
#include "torrent/download/download_manager.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/rate.h"
#include "torrent/runtime/runtime.h"
#include "torrent/utils/log.h"
#include "utils/partial_queue.h"
#endif

namespace torrent::webtorrent {

WebtorrentManager::WebtorrentManager(DownloadMain* download) : m_download(download) {
}

WebtorrentManager::~WebtorrentManager() {
  close();
}

void
WebtorrentManager::start() {
  m_active = true;
}

void
WebtorrentManager::stop() {
  m_active = false;
  close();
}

#ifdef USE_WEBTORRENT

namespace {

constexpr const char* protocol_name = "BitTorrent protocol";
constexpr uint32_t handshake_size = 68;
constexpr uint32_t block_size = 1 << 14;
constexpr uint32_t max_request_size = 1 << 20;
constexpr uint8_t extension_bit = 0x10;
constexpr uint8_t local_ut_metadata_id = 1;

std::string
make_key(uint32_t index, uint32_t offset) {
  return std::to_string(index) + ":" + std::to_string(offset);
}

void
write_uint32(std::vector<char>* buffer, uint32_t value) {
  buffer->push_back(static_cast<char>((value >> 24) & 0xff));
  buffer->push_back(static_cast<char>((value >> 16) & 0xff));
  buffer->push_back(static_cast<char>((value >> 8) & 0xff));
  buffer->push_back(static_cast<char>(value & 0xff));
}

uint32_t
read_uint32(const char* data) {
  return (static_cast<uint8_t>(data[0]) << 24) |
         (static_cast<uint8_t>(data[1]) << 16) |
         (static_cast<uint8_t>(data[2]) << 8) |
         static_cast<uint8_t>(data[3]);
}

std::vector<char>
make_metadata_message(DownloadMain* download, size_t piece) {
  const size_t metadata_size = download->info()->metadata_size();
  const size_t piece_end = (metadata_size + ProtocolExtension::metadata_piece_size - 1) >> ProtocolExtension::metadata_piece_shift;

  if (download->info()->is_meta_download() || metadata_size == 0 || piece >= piece_end) {
    std::ostringstream reject;
    reject << "d8:msg_typei2e5:piecei" << piece << "ee";
    auto str = reject.str();
    return std::vector<char>(str.begin(), str.end());
  }

  auto wrapper_itr = manager->download_manager()->find(download->info());

  if (wrapper_itr == manager->download_manager()->end())
    return {};

  std::vector<char> metadata(metadata_size);
  object_write_bencode_c(object_write_to_buffer, nullptr, object_buffer_t(metadata.data(), metadata.data() + metadata.size()),
                         &(*wrapper_itr)->bencode()->get_key("info"));

  const size_t length = piece == piece_end - 1 ? metadata_size - (piece << ProtocolExtension::metadata_piece_shift) : ProtocolExtension::metadata_piece_size;

  std::ostringstream header;
  header << "d8:msg_typei1e5:piecei" << piece << "e10:total_sizei" << metadata_size << "ee";

  auto header_str = header.str();
  std::vector<char> result(header_str.begin(), header_str.end());
  result.insert(result.end(),
                metadata.begin() + (piece << ProtocolExtension::metadata_piece_shift),
                metadata.begin() + (piece << ProtocolExtension::metadata_piece_shift) + length);
  return result;
}

} // namespace

class WebtorrentSession : public std::enable_shared_from_this<WebtorrentSession> {
public:
  WebtorrentSession(WebtorrentManager* manager, DownloadMain* download, RtcStream stream)
    : m_manager(manager),
      m_download(download),
      m_stream(std::make_unique<DataChannelStream>(std::move(stream))) {
  }

  ~WebtorrentSession() {
    close();
  }

  void start() {
    auto weak_self = weak_from_this();

    m_stream->slot_readable([weak_self] {
        net_thread::callback([weak_self] {
            if (auto self = weak_self.lock())
              self->pump();
          });
      });

    m_stream->slot_writable([weak_self] {
        net_thread::callback([weak_self] {
            if (auto self = weak_self.lock())
              self->pump();
          });
      });

    m_stream->slot_closed([weak_self] {
        net_thread::callback([weak_self] {
            if (auto self = weak_self.lock())
              self->close();
          });
      });

    queue_handshake();
    pump();
  }

  bool is_closed() const {
    return m_closed;
  }

  void close() {
    if (m_closed)
      return;

    m_closed = true;

    if (m_stream)
      m_stream->close();

    release_download_chunk();

    for (auto& entry : m_pending_transfers)
      Block::release(entry.second);

    m_pending_transfers.clear();
  }

  void pump() {
    if (m_closed || !runtime::webtorrent_enabled() || !m_stream || !m_stream->is_open()) {
      close();
      return;
    }

    flush_write_buffer();
    read_available();

    while (!m_closed && process_read_buffer())
      ;

    if (!m_closed) {
      request_more();
      flush_write_buffer();
    }
  }

private:
  void queue_handshake() {
    std::array<char, handshake_size> buffer{};

    buffer[0] = 19;
    std::memcpy(buffer.data() + 1, protocol_name, 19);
    buffer[25] |= extension_bit;
    std::memcpy(buffer.data() + 28, m_download->info()->hash().c_str(), 20);
    std::memcpy(buffer.data() + 48, m_download->info()->local_id().c_str(), 20);

    queue_bytes(buffer.data(), buffer.size());
  }

  void queue_initial_messages() {
    queue_extension_handshake();
    queue_bitfield();
    queue_message(ProtocolBase::UNCHOKE, {});
  }

  void queue_extension_handshake() {
    std::ostringstream message;

    message << "d1:md11:ut_metadatai" << static_cast<int>(local_ut_metadata_id) << "ee";

    if (!m_download->info()->is_meta_download() && m_download->info()->metadata_size() != 0)
      message << "13:metadata_sizei" << m_download->info()->metadata_size() << "e";

    message << "4:reqqi2048e1:v" << std::strlen("libTorrent " VERSION) << ":libTorrent " VERSION << "e";

    auto payload = message.str();
    std::vector<char> body;
    body.reserve(payload.size() + 1);
    body.push_back(ProtocolExtension::HANDSHAKE);
    body.insert(body.end(), payload.begin(), payload.end());
    queue_message(ProtocolBase::EXTENSION_PROTOCOL, body);
  }

  void queue_bitfield() {
    const auto* bitfield = m_download->file_list()->bitfield();

    if (bitfield->is_all_unset())
      return;

    std::vector<char> body;
    body.reserve(bitfield->size_bytes() + 1);
    body.push_back(ProtocolBase::BITFIELD);
    body.insert(body.end(), bitfield->begin(), bitfield->end());
    queue_sized(body);

    for (uint32_t index = 0; index < bitfield->size_bits(); ++index) {
      if (!bitfield->get(index))
        continue;

      std::vector<char> have;
      have.push_back(ProtocolBase::HAVE);
      write_uint32(&have, index);
      queue_sized(have);
    }
  }

  void queue_message(uint8_t type, const std::vector<char>& payload) {
    std::vector<char> body;
    body.reserve(payload.size() + 1);
    body.push_back(type);
    body.insert(body.end(), payload.begin(), payload.end());
    queue_sized(body);
  }

  void queue_sized(const std::vector<char>& body) {
    write_uint32(&m_write_buffer, body.size());
    m_write_buffer.insert(m_write_buffer.end(), body.begin(), body.end());
  }

  void queue_bytes(const void* data, size_t size) {
    auto first = static_cast<const char*>(data);
    m_write_buffer.insert(m_write_buffer.end(), first, first + size);
  }

  bool flush_write_buffer() {
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

  void read_available() {
    while (!m_closed && m_stream->available() != 0) {
      char buffer[4096];
      int length = m_stream->read_stream(buffer, std::min<size_t>(sizeof(buffer), m_stream->available()));

      if (length <= 0)
        return;

      m_read_buffer.insert(m_read_buffer.end(), buffer, buffer + length);
    }
  }

  bool process_read_buffer() {
    if (!m_handshake_read)
      return process_handshake();

    if (m_read_buffer.size() < 4)
      return false;

    uint32_t length = read_uint32(m_read_buffer.data());

    if (length > max_request_size + 9) {
      close();
      return false;
    }

    if (m_read_buffer.size() < length + 4)
      return false;

    if (length != 0)
      process_message(static_cast<uint8_t>(m_read_buffer[4]), m_read_buffer.data() + 5, length - 1);

    m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + length + 4);
    return true;
  }

  bool process_handshake() {
    if (m_read_buffer.size() < handshake_size)
      return false;

    if (static_cast<uint8_t>(m_read_buffer[0]) != 19 ||
        std::memcmp(m_read_buffer.data() + 1, protocol_name, 19) != 0 ||
        m_download->info()->hash().not_equal_to(m_read_buffer.data() + 28)) {
      close();
      return false;
    }

    m_remote_extensions = (static_cast<uint8_t>(m_read_buffer[25]) & extension_bit) != 0;
    m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + handshake_size);
    m_handshake_read = true;
    queue_initial_messages();
    return true;
  }

  void process_message(uint8_t type, const char* payload, uint32_t length) {
    switch (type) {
    case ProtocolBase::CHOKE:
      m_remote_choked = true;
      break;
    case ProtocolBase::UNCHOKE:
      m_remote_choked = false;
      break;
    case ProtocolBase::HAVE:
      if (length == 4)
        receive_have(read_uint32(payload));
      break;
    case ProtocolBase::BITFIELD:
      receive_bitfield(payload, length);
      break;
    case ProtocolBase::REQUEST:
      if (length == 12)
        receive_request(Piece(read_uint32(payload), read_uint32(payload + 4), read_uint32(payload + 8)));
      break;
    case ProtocolBase::PIECE:
      if (length >= 8)
        receive_piece(Piece(read_uint32(payload), read_uint32(payload + 4), length - 8), payload + 8);
      break;
    case ProtocolBase::CANCEL:
      break;
    case ProtocolBase::EXTENSION_PROTOCOL:
      if (length >= 1)
        receive_extension(static_cast<uint8_t>(payload[0]), payload + 1, length - 1);
      break;
    default:
      break;
    }
  }

  void receive_have(uint32_t index) {
    if (m_remote_bitfield.empty())
      initialize_remote_bitfield();

    if (index < m_remote_bitfield.size_bits())
      m_remote_bitfield.set(index);

    maybe_send_interested();
  }

  void receive_bitfield(const char* payload, uint32_t length) {
    initialize_remote_bitfield();

    if (length != m_remote_bitfield.size_bytes()) {
      close();
      return;
    }

    std::memcpy(m_remote_bitfield.begin(), payload, length);
    m_remote_bitfield.update();

    if (!m_remote_bitfield.is_tail_cleared())
      close();
    else
      maybe_send_interested();
  }

  void initialize_remote_bitfield() {
    if (!m_remote_bitfield.empty())
      return;

    m_remote_bitfield.set_size_bits(m_download->file_list()->bitfield()->size_bits());
    m_remote_bitfield.allocate();
    m_remote_bitfield.unset_all();
  }

  void receive_extension(uint8_t type, const char* payload, uint32_t length) {
    if (type == ProtocolExtension::HANDSHAKE) {
      receive_extension_handshake(payload, length);
      return;
    }

    if (type == local_ut_metadata_id)
      receive_metadata_request(payload, length);
  }

  void receive_extension_handshake(const char* payload, uint32_t length) {
    try {
      Object object;
      object_read_bencode_c(payload, payload + length, &object);

      if (object.is_map() && object.has_key_map("m")) {
        const auto& extensions = object.get_key("m");

        if (extensions.has_key_value("ut_metadata"))
          m_remote_ut_metadata_id = static_cast<uint8_t>(extensions.get_key("ut_metadata").as_value());
      }
    } catch (const bencode_error&) {
      return;
    }
  }

  void receive_metadata_request(const char* payload, uint32_t length) {
    if (m_remote_ut_metadata_id == 0)
      return;

    try {
      Object object;
      object_read_bencode_c(payload, payload + length, &object);

      if (!object.is_map() || !object.has_key_value("msg_type") || object.get_key("msg_type").as_value() != 0 ||
          !object.has_key_value("piece"))
        return;

      auto metadata = make_metadata_message(m_download, object.get_key("piece").as_value());

      if (metadata.empty())
        return;

      std::vector<char> body;
      body.reserve(metadata.size() + 1);
      body.push_back(m_remote_ut_metadata_id);
      body.insert(body.end(), metadata.begin(), metadata.end());
      queue_message(ProtocolBase::EXTENSION_PROTOCOL, body);
    } catch (const bencode_error&) {
      return;
    }
  }

  void receive_request(const Piece& piece) {
    if (!m_download->file_list()->is_valid_piece(piece) ||
        !m_download->file_list()->bitfield()->get(piece.index()) ||
        piece.length() > max_request_size)
      return;

    ChunkHandle handle = m_download->chunk_list()->get(piece.index(), ChunkList::get_not_hashing);

    if (!handle.is_valid())
      return;

    std::vector<char> body;
    body.reserve(piece.length() + 9);
    body.push_back(ProtocolBase::PIECE);
    write_uint32(&body, piece.index());
    write_uint32(&body, piece.offset());

    const size_t old_size = body.size();
    body.resize(old_size + piece.length());
    handle.chunk()->to_buffer(body.data() + old_size, piece.offset(), piece.length());
    m_download->chunk_list()->release(&handle, ChunkList::release_default);

    queue_sized(body);
    m_download->info()->mutable_up_rate()->insert(piece.length());
  }

  void receive_piece(const Piece& piece, const char* data) {
    auto itr = m_pending_transfers.find(make_key(piece.index(), piece.offset()));

    if (itr == m_pending_transfers.end())
      return;

    BlockTransfer* transfer = itr->second;

    if (!transfer->is_valid() || transfer->piece().length() != piece.length()) {
      m_pending_transfers.erase(itr);
      Block::release(transfer);
      return;
    }

    if (!m_down_chunk.is_valid() || m_down_chunk.index() != piece.index()) {
      release_download_chunk();
      m_down_chunk = m_download->chunk_list()->get(piece.index(), ChunkList::get_not_hashing | ChunkList::get_writable);
    }

    if (!m_down_chunk.is_valid()) {
      close();
      return;
    }

    m_down_chunk.chunk()->from_buffer(data, piece.offset(), piece.length());
    transfer->adjust_position(piece.length());

    if (transfer->is_finished()) {
      m_pending_transfers.erase(itr);
      m_download->delegator()->transfer_list()->finished(transfer);
      m_down_chunk.object()->set_time_modified(this_thread::cached_time());
    }

    m_download->info()->mutable_down_rate()->insert(piece.length());
  }

  void request_more() {
    if (m_remote_choked || m_remote_bitfield.empty())
      return;

    while (m_pending_transfers.size() < 64) {
      BlockTransfer* transfer = next_transfer();

      if (transfer == nullptr)
        return;

      if (!transfer->block()->transfering(transfer)) {
        Block::release(transfer);
        continue;
      }

      m_pending_transfers[make_key(transfer->piece().index(), transfer->piece().offset())] = transfer;

      std::vector<char> body;
      body.push_back(ProtocolBase::REQUEST);
      write_uint32(&body, transfer->piece().index());
      write_uint32(&body, transfer->piece().offset());
      write_uint32(&body, transfer->piece().length());
      queue_sized(body);
    }
  }

  void maybe_send_interested() {
    if (m_sent_interested || m_remote_bitfield.empty())
      return;

    const auto* local_bitfield = m_download->file_list()->bitfield();

    for (uint32_t index = 0; index < m_remote_bitfield.size_bits(); ++index) {
      if (!m_remote_bitfield.get(index) || local_bitfield->get(index) || !m_download->chunk_selector()->is_wanted(index))
        continue;

      queue_message(ProtocolBase::INTERESTED, {});
      m_sent_interested = true;
      return;
    }
  }

  BlockTransfer* next_transfer() {
    auto transfer_list = m_download->delegator()->transfer_list();

    for (auto block_list : *transfer_list) {
      if (!m_remote_bitfield.get(block_list->index()))
        continue;

      for (auto& block : *block_list) {
        if (!block.is_finished() && block.is_stalled() && block.size_all() == 0)
          return block.insert(nullptr);
      }
    }

    uint32_t index = m_download->chunk_selector()->find(&m_remote_bitfield, &m_cache, false);

    if (index == ChunkSelector::invalid_chunk)
      return nullptr;

    auto block_list_itr = transfer_list->insert(Piece(index, 0, m_download->file_list()->chunk_index_size(index)), block_size);

    for (auto& block : **block_list_itr) {
      if (!block.is_finished())
        return block.insert(nullptr);
    }

    return nullptr;
  }

  void release_download_chunk() {
    if (m_down_chunk.is_valid())
      m_download->chunk_list()->release(&m_down_chunk, ChunkList::release_default);
  }

  WebtorrentManager* m_manager{};
  DownloadMain* m_download{};
  std::unique_ptr<DataChannelStream> m_stream;

  Bitfield m_remote_bitfield;
  utils::PartialQueue m_cache;
  ChunkHandle m_down_chunk;
  std::map<std::string, BlockTransfer*> m_pending_transfers;

  std::vector<char> m_read_buffer;
  std::vector<char> m_write_buffer;
  size_t m_write_position{};

  bool m_closed{false};
  bool m_handshake_read{false};
  bool m_remote_extensions{false};
  bool m_remote_choked{true};
  bool m_sent_interested{false};
  uint8_t m_remote_ut_metadata_id{0};
};

void
WebtorrentManager::close() {
  for (auto& session : m_sessions)
    if (session)
      session->close();

  m_sessions.clear();
}

void
WebtorrentManager::tick() {
  auto last = std::remove_if(m_sessions.begin(), m_sessions.end(), [](const auto& session) {
      return !session || session->is_closed();
    });

  m_sessions.erase(last, m_sessions.end());

  for (auto& session : m_sessions)
    session->pump();
}

void
WebtorrentManager::receive_stream(RtcStream stream) {
  if (!m_active || !runtime::webtorrent_enabled())
    return;

  auto session = std::make_shared<WebtorrentSession>(this, m_download, std::move(stream));
  m_sessions.push_back(session);
  session->start();
}

#else

void
WebtorrentManager::close() {
}

void
WebtorrentManager::tick() {
}

#endif // USE_WEBTORRENT

} // namespace torrent::webtorrent
