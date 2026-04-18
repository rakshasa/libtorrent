#ifndef LIBTORRENT_TORRENT_EVENT_H
#define LIBTORRENT_TORRENT_EVENT_H

#include <memory>
#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent {

namespace net {
class PollEvent;
class PollInternal;
}

class LIBTORRENT_EXPORT Event {
public:
  virtual             ~Event();

  bool                is_open() const;
  bool                is_polling() const;

  int                 file_descriptor() const;
  int                 socket_type_or_zero() const;

  auto                peer_address() const;
  auto                socket_address() const;

  std::string         print_name_fd_str() const;

  virtual const char* type_name() const;

  // TODO: Make these protected.
  virtual void        event_read() = 0;
  virtual void        event_write() = 0;
  virtual void        event_error() = 0;

  // TODO: Add bool event_fd_reused().

protected:
  friend class net::Poll;
  friend class net::PollInternal;
  friend class runtime::SocketManager;

  void                set_file_descriptor(int fd);

  bool                update_socket_address();
  bool                update_peer_address();

  // Returns true if the update failed and the address was null.
  bool                update_and_verify_socket_address();
  bool                update_and_verify_peer_address();

  std::shared_ptr<net::PollEvent> m_poll_event;

  int                 m_fileDesc{-1};

private:
  // TODO: Add socket type to validation.
  c_sa_unique_ptr     m_peer_address;
  c_sa_unique_ptr     m_socket_address;
};

inline bool Event::is_open() const             { return file_descriptor() != -1; }
inline bool Event::is_polling() const          { return m_poll_event != nullptr; }

inline int  Event::file_descriptor() const     { return m_fileDesc; }
inline void Event::set_file_descriptor(int fd) { m_fileDesc = fd; }

inline auto Event::peer_address() const        { return m_peer_address.get(); }
inline auto Event::socket_address() const      { return m_socket_address.get(); }

} // namespace torrent

#endif
