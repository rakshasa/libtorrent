#include "config.h"

#include "test/helpers/network.h"

#include <sys/select.h>

#include "torrent/event.h"

bool
check_event_is_readable(torrent::Event* event, std::chrono::microseconds timeout) {
  int fd = event->file_descriptor();

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  struct timeval t = {
    static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(timeout).count()),
    static_cast<suseconds_t>(timeout.count() % 1000000)
  };

  int result = select(fd + 1, &read_fds, nullptr, nullptr, &t);

  CPPUNIT_ASSERT(result != -1);

  return FD_ISSET(fd, &read_fds);
}
