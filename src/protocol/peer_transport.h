#ifndef LIBTORRENT_PROTOCOL_PEER_TRANSPORT_H
#define LIBTORRENT_PROTOCOL_PEER_TRANSPORT_H

#include <cstdint>
#include <memory>

namespace torrent {

class PeerConnectionBase;

namespace webtorrent {
class DataChannelStream;
} // namespace webtorrent

class PeerTransport {
public:
  enum Type {
    tcp,
    webtorrent,
  };

  virtual ~PeerTransport();

  static std::unique_ptr<PeerTransport> create_tcp(int fd);
  static std::unique_ptr<PeerTransport> create_webtorrent(std::unique_ptr<webtorrent::DataChannelStream> stream);

  virtual Type type() const = 0;
  virtual int file_descriptor() const = 0;
  virtual bool is_open() const = 0;

  virtual void bind(PeerConnectionBase* connection) = 0;
  virtual void close(PeerConnectionBase* connection) = 0;

  virtual int read_stream(void* buffer, uint32_t length) = 0;
  virtual int write_stream(const void* buffer, uint32_t length) = 0;

  virtual void insert_read(PeerConnectionBase* connection) = 0;
  virtual void insert_write(PeerConnectionBase* connection) = 0;
  virtual void remove_read(PeerConnectionBase* connection) = 0;
  virtual void remove_write(PeerConnectionBase* connection) = 0;

  virtual uint32_t read_buffer_size(uint32_t socket_size, uint32_t buffer_size) const = 0;
  virtual bool allows_upload_while_choked() const = 0;
  virtual uint32_t max_request_size() const = 0;
  virtual void schedule_write_retry() = 0;
  virtual bool retry_after_piece_write() = 0;
};

} // namespace torrent

#endif
