#ifndef LIBTORRENT_TORRENT_EVENT_H
#define LIBTORRENT_TORRENT_EVENT_H

#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT Event {
public:
  virtual             ~Event() = default;

  bool                is_open() const;

  int                 file_descriptor() const;

  // TODO: Require all to define their own typename.
  virtual const char* type_name() const;

  // TODO: Make these protected.
  virtual void        event_read() = 0;
  virtual void        event_write() = 0;
  virtual void        event_error() = 0;


protected:
  void                close_file_descriptor();
  void                set_file_descriptor(int fd);

  // TODO: Rename to m_fd.
  int                 m_fileDesc{-1};

  // TODO: Deprecate.
  bool                m_ipv6_socket{false};
};

inline bool Event::is_open() const             { return file_descriptor() != -1; }
inline int  Event::file_descriptor() const     { return m_fileDesc; }
inline void Event::set_file_descriptor(int fd) { m_fileDesc = fd; }

} // namespace torrent

#endif
