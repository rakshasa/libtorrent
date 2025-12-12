#ifndef LIBTORRENT_TORRENT_EVENT_H
#define LIBTORRENT_TORRENT_EVENT_H

#include <memory>
#include <torrent/common.h>

namespace torrent {

namespace net {
class PollEvent;
class PollInternal;
}

class LIBTORRENT_EXPORT Event {
public:
  virtual             ~Event();

  bool                is_open() const;

  int                 file_descriptor() const;

  std::string         print_name_fd_str() const;

  virtual const char* type_name() const = 0;

  // TODO: Make these protected.
  virtual void        event_read() = 0;
  virtual void        event_write() = 0;
  virtual void        event_error() = 0;

protected:
  friend class net::Poll;
  friend class net::PollInternal;

  void                close_file_descriptor();
  void                set_file_descriptor(int fd);

  std::shared_ptr<net::PollEvent> m_poll_event;

  // TODO: replace by m_poll_event->fd()
  int                 m_fileDesc{-1};

  // TODO: Deprecate.
  bool                m_ipv6_socket{false};
};

inline bool Event::is_open() const             { return file_descriptor() != -1; }
inline int  Event::file_descriptor() const     { return m_fileDesc; }
inline void Event::set_file_descriptor(int fd) { m_fileDesc = fd; }

} // namespace torrent

#endif
