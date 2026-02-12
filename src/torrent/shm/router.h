#ifndef LIBTORRENT_TORRENT_SHM_ROUTER_H
#define LIBTORRENT_TORRENT_SHM_ROUTER_H

#include <map>
#include <torrent/common.h>

// Uses read and write shm::Channel for inter-process communication.
//
// Use ids to distinguish different users of the channel, with the top bits for control
// purposes. (open/close etc)
//
// Channel::header_type is passed to the reader to efficiently read data, and the reader tells
// Router when it is consumed.
//
// We store id's with std::function handlers, and for now just use a simple container for that.
//
// We may later want to optimize this with a more efficient container. So add member functions for access.
//
// Router should have one channel producer, and one consumer, to avoid id conflicts.

namespace torrent::shm {

// Add to common.h
class Channel;

struct RouterHandler {
  // We use on_error to indicate close() was called on this side, as we never call on_error after
  // close(). (nor on_read)
  bool is_closed_read()  { return on_read == nullptr; }
  bool is_closed_write() { return on_error == nullptr; }

  // Use size=0 to indicate close.
  //
  // Set on_read to nullptr to indicate closed from this side.
  std::function<void(void* data, uint32_t size)> on_read;
  std::function<void(void* data, uint32_t size)> on_error;

  // TODO: add handler for when to resume after write failure due to full channel.
};

class LIBTORRENT_EXPORT Router {
public:
  using data_func = std::function<void(void* data, uint32_t size)>;

  constexpr static uint32_t flag_close = 0x80000000;
  constexpr static uint32_t flag_mask  = 0xF0000000;

  Router(int fd, Channel* read_channel, Channel* write_channel);
  ~Router();

  int                 file_descriptor() const;

  // TODO: Replace uint32_t with struct with member functions.
  uint32_t            register_handler(data_func on_read, data_func on_error);
  void                register_handler(int id, data_func on_read, data_func on_error);
  bool                try_register_handler(int id, data_func on_read, data_func on_error);

  void                close(uint32_t id);

  // TODO: Add direct write to shm channel from at-write generated data.

  bool                write(uint32_t id, uint32_t size, void* data);

  void                process_reads();

  void                send_fatal_error(const char* msg, uint32_t size);

private:
  using handler_map = std::map<uint32_t, RouterHandler>;

  Channel*            m_read_channel{};
  Channel*            m_write_channel{};

  // TODO: Make sighandler send stack dump on crash?
  int                 m_fd;

  uint32_t            m_next_id{1};
  handler_map         m_handlers;
};

inline int Router::file_descriptor() const { return m_fd; }

} // namespace torrent::shm

#endif // LIBTORRENT_TORRENT_SHM_CHANNEL_H
