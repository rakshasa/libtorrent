#include "config.h"

#ifdef USE_WEBTORRENT

#include "webtorrent/data_channel_stream.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <vector>

namespace torrent::webtorrent {

namespace {

constexpr size_t data_channel_buffer_low_watermark = 128 * 1024;
constexpr size_t data_channel_buffer_high_watermark = 256 * 1024;
constexpr size_t data_channel_write_queue_limit = 1024 * 1024;

} // namespace

DataChannelStream::DataChannelStream(RtcStream stream)
  : m_stream(std::move(stream)) {
  if (!m_stream.data_channel)
    return;

  m_stream.data_channel->setBufferedAmountLowThreshold(data_channel_buffer_low_watermark);
  m_stream.data_channel->onMessage([this](rtc::message_variant data) { receive_message(std::move(data)); });
  m_stream.data_channel->onBufferedAmountLow([this] { receive_writable(); });
  m_stream.data_channel->onClosed([this] { receive_closed(); });
  m_stream.data_channel->onError([this]([[maybe_unused]] std::string error) { receive_closed(); });
}

DataChannelStream::~DataChannelStream() {
  close();
}

bool
DataChannelStream::is_open() const {
  std::scoped_lock guard(m_mutex);
  return !m_closed && m_stream.data_channel && m_stream.data_channel->isOpen();
}

size_t
DataChannelStream::available() const {
  std::scoped_lock guard(m_mutex);
  return m_read_buffer.size();
}

size_t
DataChannelStream::pending_write() const {
  std::scoped_lock guard(m_mutex);
  return m_write_buffer.size();
}

int
DataChannelStream::read_stream(void* buffer, uint32_t length) {
  std::scoped_lock guard(m_mutex);

  if (length == 0)
    return 0;

  if (m_read_buffer.empty()) {
    errno = EAGAIN;
    return -1;
  }

  auto writable = static_cast<char*>(buffer);
  const auto bytes = std::min<size_t>(length, m_read_buffer.size());

  for (size_t i = 0; i < bytes; ++i) {
    writable[i] = m_read_buffer.front();
    m_read_buffer.pop_front();
  }

  return static_cast<int>(bytes);
}

int
DataChannelStream::write_stream(const void* buffer, uint32_t length) {
  std::shared_ptr<rtc::DataChannel> data_channel;

  {
    std::scoped_lock guard(m_mutex);

    if (length == 0)
      return 0;

    if (m_closed || !m_stream.data_channel || !m_stream.data_channel->isOpen())
      return 0;

    if (m_write_buffer.size() + length > data_channel_write_queue_limit) {
      errno = EAGAIN;
      return -1;
    }

    const auto* readable = static_cast<const char*>(buffer);
    m_write_buffer.insert(m_write_buffer.end(), readable, readable + length);
    data_channel = m_stream.data_channel;
  }

  flush_write_buffer();
  return static_cast<int>(length);
}

void
DataChannelStream::slot_readable(slot_type slot) {
  std::scoped_lock guard(m_mutex);
  m_slot_readable = std::move(slot);
}

void
DataChannelStream::slot_writable(slot_type slot) {
  std::scoped_lock guard(m_mutex);
  m_slot_writable = std::move(slot);
}

void
DataChannelStream::slot_closed(slot_type slot) {
  std::scoped_lock guard(m_mutex);
  m_slot_closed = std::move(slot);
}

void
DataChannelStream::flush_write_buffer() {
  std::shared_ptr<rtc::DataChannel> data_channel;
  const auto clear_flushing = [this] {
    std::scoped_lock guard(m_mutex);
    m_write_flushing = false;
  };

  {
    std::scoped_lock guard(m_mutex);

    if (m_closed || !m_stream.data_channel || !m_stream.data_channel->isOpen()) {
      m_write_flushing = false;
      return;
    }

    if (m_write_flushing)
      return;

    m_write_flushing = true;
    data_channel = m_stream.data_channel;
  }

  const size_t configured_message_size = data_channel->maxMessageSize();
  const size_t max_message_size = configured_message_size != 0 ? std::min<size_t>(configured_message_size, 16 * 1024) : 16 * 1024;

  while (data_channel->bufferedAmount() < data_channel_buffer_high_watermark) {
    std::vector<std::byte> chunk;

    {
      std::scoped_lock guard(m_mutex);

      if (m_closed || m_write_buffer.empty()) {
        m_write_flushing = false;
        return;
      }

      const auto chunk_size = std::min(max_message_size, m_write_buffer.size());
      chunk.reserve(chunk_size);

      for (size_t i = 0; i < chunk_size; ++i)
        chunk.push_back(static_cast<std::byte>(m_write_buffer[i]));
    }

    if (!data_channel->send(reinterpret_cast<const rtc::byte*>(chunk.data()), chunk.size())) {
      clear_flushing();
      return;
    }

    {
      std::scoped_lock guard(m_mutex);

      if (m_write_buffer.size() < chunk.size()) {
        m_write_flushing = false;
        return;
      }

      m_write_buffer.erase(m_write_buffer.begin(), m_write_buffer.begin() + chunk.size());
    }
  }

  clear_flushing();
}

void
DataChannelStream::close() {
  std::shared_ptr<rtc::DataChannel> data_channel;
  slot_type slot_closed;

  {
    std::scoped_lock guard(m_mutex);

    if (m_closed)
      return;

    m_closed = true;
    data_channel = m_stream.data_channel;
    m_write_buffer.clear();
    slot_closed = m_slot_closed;
  }

  if (data_channel) {
    data_channel->resetCallbacks();
    data_channel->close();
  }

  if (slot_closed)
    slot_closed();
}

void
DataChannelStream::receive_message(rtc::message_variant data) {
  slot_type slot_readable;

  {
    std::scoped_lock guard(m_mutex);

    if (auto binary = std::get_if<rtc::binary>(&data)) {
      for (auto byte : *binary)
        m_read_buffer.push_back(static_cast<char>(byte));
    } else if (auto message = std::get_if<std::string>(&data)) {
      m_read_buffer.insert(m_read_buffer.end(), message->begin(), message->end());
    }

    slot_readable = m_slot_readable;
  }

  if (slot_readable)
    slot_readable();
}

void
DataChannelStream::receive_writable() {
  slot_type slot_writable;

  flush_write_buffer();

  {
    std::scoped_lock guard(m_mutex);
    slot_writable = m_slot_writable;
  }

  if (slot_writable)
    slot_writable();
}

void
DataChannelStream::receive_closed() {
  slot_type slot_closed;

  {
    std::scoped_lock guard(m_mutex);

    if (m_closed)
      return;

    m_closed = true;
    slot_closed = m_slot_closed;
  }

  if (slot_closed)
    slot_closed();
}

} // namespace torrent::webtorrent

#endif // USE_WEBTORRENT
