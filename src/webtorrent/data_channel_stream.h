#ifndef LIBTORRENT_WEBTORRENT_DATA_CHANNEL_STREAM_H
#define LIBTORRENT_WEBTORRENT_DATA_CHANNEL_STREAM_H

#include "config.h"

#ifdef USE_WEBTORRENT

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <rtc/rtc.hpp>

#include "webtorrent/rtc_signaling.h"

namespace torrent::webtorrent {

class DataChannelStream {
public:
  using slot_type = std::function<void()>;

  explicit DataChannelStream(RtcStream stream);
  ~DataChannelStream();

  DataChannelStream(const DataChannelStream&) = delete;
  DataChannelStream& operator=(const DataChannelStream&) = delete;

  bool is_open() const;
  size_t available() const;
  size_t pending_write() const;

  int read_stream(void* buffer, uint32_t length);
  int write_stream(const void* buffer, uint32_t length);

  void close();

  void slot_readable(slot_type slot);
  void slot_writable(slot_type slot);
  void slot_closed(slot_type slot);

private:
  void flush_write_buffer();

  void receive_message(rtc::message_variant data);
  void receive_writable();
  void receive_closed();

  mutable std::mutex m_mutex;
  RtcStream m_stream;
  std::deque<char> m_read_buffer;
  std::deque<char> m_write_buffer;
  bool m_closed{false};
  bool m_write_flushing{false};

  slot_type m_slot_readable;
  slot_type m_slot_writable;
  slot_type m_slot_closed;
};

} // namespace torrent::webtorrent

#else

#include <cstdint>
#include <functional>

namespace torrent::webtorrent {

class DataChannelStream {
public:
  using slot_type = std::function<void()>;

  bool is_open() const { return false; }
  size_t available() const { return 0; }
  size_t pending_write() const { return 0; }

  int read_stream(void*, uint32_t) { return 0; }
  int write_stream(const void*, uint32_t) { return 0; }

  void close() {}

  void slot_readable(slot_type) {}
  void slot_writable(slot_type) {}
  void slot_closed(slot_type) {}
};

} // namespace torrent::webtorrent

#endif

#endif // LIBTORRENT_WEBTORRENT_DATA_CHANNEL_STREAM_H
