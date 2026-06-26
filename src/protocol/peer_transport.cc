#include "config.h"

#include "protocol/peer_transport.h"

#include <cerrno>
#include <sys/socket.h>

#include "protocol/peer_connection_base.h"
#include "thread_main.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/callbacks.h"
#include "torrent/system/poll.h"
#include "torrent/utils/scheduler.h"

#include "webtorrent/data_channel_stream.h"

namespace torrent {

PeerTransport::~PeerTransport() = default;

namespace {

class TcpPeerTransport : public PeerTransport {
public:
  explicit TcpPeerTransport(int fd) : m_fd(fd) {}

  Type type() const override { return tcp; }
  int file_descriptor() const override { return m_fd; }
  bool is_open() const override { return m_fd >= 0; }

  void bind(PeerConnectionBase* connection) override {
    if (m_fd < 0)
      throw internal_error("TcpPeerTransport::bind() received invalid fd.");

    this_thread::poll()->open(connection);
    this_thread::poll()->insert_read(connection);
    this_thread::poll()->insert_write(connection);
    this_thread::poll()->insert_error(connection);
  }

  void close(PeerConnectionBase* connection) override {
    runtime::socket_manager()->close_event_or_throw(connection, [connection, this]() {
        this_thread::poll()->remove_and_close(connection);

        fd_close(m_fd);
        m_fd = -1;
      });
  }

  int read_stream(void* buffer, uint32_t length) override {
    return ::recv(m_fd, buffer, length, 0);
  }

  int write_stream(const void* buffer, uint32_t length) override {
    return ::send(m_fd, buffer, length, 0);
  }

  void insert_read(PeerConnectionBase* connection) override { this_thread::poll()->insert_read(connection); }
  void insert_write(PeerConnectionBase* connection) override { this_thread::poll()->insert_write(connection); }
  void remove_read(PeerConnectionBase* connection) override { this_thread::poll()->remove_read(connection); }
  void remove_write(PeerConnectionBase* connection) override { this_thread::poll()->remove_write(connection); }

  uint32_t read_buffer_size(uint32_t socket_size, uint32_t) const override { return socket_size; }
  bool allows_upload_while_choked() const override { return false; }
  uint32_t max_request_size() const override { return 1 << 17; }
  void schedule_write_retry() override {}
  bool retry_after_piece_write() override { return false; }

private:
  int m_fd;
};

#ifdef USE_WEBTORRENT

class WebtorrentPeerTransport : public PeerTransport {
public:
  explicit WebtorrentPeerTransport(std::unique_ptr<webtorrent::DataChannelStream> stream)
    : m_stream(std::move(stream)),
      m_alive(std::make_shared<bool>(true)) {
  }

  ~WebtorrentPeerTransport() override {
    shutdown();
  }

  Type type() const override { return webtorrent; }
  int file_descriptor() const override { return -2; }
  bool is_open() const override { return m_stream && m_stream->is_open(); }

  void bind(PeerConnectionBase* connection) override {
    if (!m_stream || !m_stream->is_open())
      throw internal_error("WebtorrentPeerTransport::bind() received an invalid stream.");

    m_connection = connection;
    m_write_retry.slot() = [this, alive = m_alive] {
        if (*alive && m_connection != nullptr && m_connection->is_open())
          schedule_write();
      };

    m_stream->slot_readable([this, alive = m_alive] {
        main_thread::callback([this, alive] {
            if (*alive && m_connection != nullptr && m_connection->is_open())
              schedule_read();
          });
      });

    m_stream->slot_writable([this, alive = m_alive] {
        main_thread::callback([this, alive] {
            if (*alive && m_connection != nullptr && m_connection->is_open())
              schedule_write();
          });
      });

    m_stream->slot_closed([this, alive = m_alive] {
        main_thread::callback([this, alive] {
            if (*alive && m_connection != nullptr && m_connection->is_open())
              m_connection->event_error();
          });
      });

    insert_read(connection);
    insert_write(connection);
  }

  void close(PeerConnectionBase*) override {
    shutdown();
  }

  int read_stream(void* buffer, uint32_t length) override {
    int result = m_stream->read_stream(buffer, length);

    if (result > 0 && m_stream->available() != 0)
      schedule_read();

    return result;
  }

  int write_stream(const void* buffer, uint32_t length) override {
    int result = m_stream->write_stream(buffer, length);

    if (result > 0 && static_cast<uint32_t>(result) < length)
      schedule_write_retry();
    else if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
      schedule_write_retry();
    else if (result > 0 && m_stream->pending_write() != 0)
      schedule_write_retry();

    return result;
  }

  void insert_read(PeerConnectionBase*) override { schedule_read(); }
  void insert_write(PeerConnectionBase*) override { schedule_write(); }
  void remove_read(PeerConnectionBase*) override {}
  void remove_write(PeerConnectionBase*) override {}

  uint32_t read_buffer_size(uint32_t, uint32_t buffer_size) const override { return buffer_size; }
  bool allows_upload_while_choked() const override { return true; }
  uint32_t max_request_size() const override { return 1 << 20; }

  void schedule_write_retry() override {
    if (m_stream && m_write_retry.is_valid())
      this_thread::scheduler()->update_wait_for(&m_write_retry, std::chrono::milliseconds(5));
  }

  bool retry_after_piece_write() override {
    schedule_write_retry();
    return true;
  }

private:
  void schedule_read() {
    if (m_read_pending)
      return;

    m_read_pending = true;

    main_thread::callback([this, alive = m_alive] {
        if (!*alive || m_connection == nullptr || !m_connection->is_open())
          return;

        m_read_pending = false;
        m_connection->event_read();
      });
  }

  void schedule_write() {
    if (m_write_pending)
      return;

    m_write_pending = true;

    main_thread::callback([this, alive = m_alive] {
        if (!*alive || m_connection == nullptr || !m_connection->is_open())
          return;

        m_write_pending = false;
        m_connection->event_write();
      });
  }

  void shutdown() {
    if (m_alive)
      *m_alive = false;

    if (m_write_retry.is_scheduled())
      this_thread::scheduler()->erase(&m_write_retry);

    if (m_stream)
      m_stream->close();

    m_stream.reset();
    m_connection = nullptr;
    m_read_pending = false;
    m_write_pending = false;
  }

  PeerConnectionBase* m_connection{};
  std::unique_ptr<webtorrent::DataChannelStream> m_stream;
  std::shared_ptr<bool> m_alive;
  utils::SchedulerEntry m_write_retry;
  bool m_read_pending{false};
  bool m_write_pending{false};
};

#else

class WebtorrentPeerTransport : public PeerTransport {
public:
  explicit WebtorrentPeerTransport(std::unique_ptr<webtorrent::DataChannelStream>) {}

  Type type() const override { return webtorrent; }
  int file_descriptor() const override { return -1; }
  bool is_open() const override { return false; }
  void bind(PeerConnectionBase*) override { throw internal_error("WebTorrent support is not available in this build."); }
  void close(PeerConnectionBase*) override {}
  int read_stream(void*, uint32_t) override { return 0; }
  int write_stream(const void*, uint32_t) override { return 0; }
  void insert_read(PeerConnectionBase*) override {}
  void insert_write(PeerConnectionBase*) override {}
  void remove_read(PeerConnectionBase*) override {}
  void remove_write(PeerConnectionBase*) override {}
  uint32_t read_buffer_size(uint32_t socket_size, uint32_t) const override { return socket_size; }
  bool allows_upload_while_choked() const override { return false; }
  uint32_t max_request_size() const override { return 1 << 17; }
  void schedule_write_retry() override {}
  bool retry_after_piece_write() override { return false; }
};

#endif

} // namespace

std::unique_ptr<PeerTransport>
PeerTransport::create_tcp(int fd) {
  return std::make_unique<TcpPeerTransport>(fd);
}

std::unique_ptr<PeerTransport>
PeerTransport::create_webtorrent(std::unique_ptr<webtorrent::DataChannelStream> stream) {
  return std::make_unique<WebtorrentPeerTransport>(std::move(stream));
}

} // namespace torrent
