#ifndef LIBTORRENT_UTILS_THREAD_INTERRUPT_H
#define LIBTORRENT_UTILS_THREAD_INTERRUPT_H

#include <atomic>
#include <utility>
#include <torrent/event.h>

namespace torrent {

class SocketFd;

class LIBTORRENT_EXPORT thread_interrupt : public Event {
public:
  typedef std::pair<thread_interrupt*, thread_interrupt*> pair_type;

  ~thread_interrupt();

  static pair_type    create_pair();

  bool                poke();

  void                event_read();
  void                event_write() {}
  void                event_error() {}

private:
  thread_interrupt(int fd);

  SocketFd&           get_fd() { return *reinterpret_cast<SocketFd*>(&m_fileDesc); }

  thread_interrupt*   m_other;
  std::atomic_flag    m_poking;
};

}

#endif
