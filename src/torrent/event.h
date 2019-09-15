#ifndef LIBTORRENT_TORRENT_EVENT_H
#define LIBTORRENT_TORRENT_EVENT_H

#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT Event {
public:
  Event();
  virtual ~Event();

  // TODO: Disable override.
  bool is_open() const;

  int file_descriptor() const;

  virtual void event_read() = 0;
  virtual void event_write() = 0;
  virtual void event_error() = 0;

  // TODO: Require all to define their ownh typename.
  virtual const char* type_name() const { return "default"; }

protected:
  void close_file_descriptor();
  void set_file_descriptor(int fd);

  int  m_fileDesc;

  // TODO: Deprecate.
  bool m_ipv6_socket;
};

inline Event::Event() : m_fileDesc(-1), m_ipv6_socket(false) {}
inline Event::~Event() {}
inline bool Event::is_open() const { return file_descriptor() != -1; }
inline int  Event::file_descriptor() const { return m_fileDesc; }
inline void Event::set_file_descriptor(int fd) { m_fileDesc = fd; }

// Defined in 'src/globals.cc'.
[[gnu::weak]] void poll_event_open(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_close(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_closed(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_insert_read(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_insert_write(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_insert_error(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_remove_read(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_remove_write(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void poll_event_remove_error(Event* event) LIBTORRENT_EXPORT;

}

#endif
