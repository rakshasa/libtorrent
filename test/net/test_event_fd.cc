#include "config.h"

#ifdef USE_EPOLL

#include "test/net/test_event_fd.h"

#include <fcntl.h>
#include <unistd.h>

#include "net/event_fd.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_event_fd, "net");

void
test_event_fd::test_destroy_closes_descriptor() {
  int fd;

  {
    torrent::net::EventFd event;
    event.add_to_poll();
    fd = event.file_descriptor();

    event.remove_from_poll(torrent::this_thread::poll());
  }

  CPPUNIT_ASSERT(::fcntl(fd, F_GETFD) == -1);
}

void
test_event_fd::test_destroy_closes_standard_descriptor() {
  int saved_stdin = ::dup(STDIN_FILENO);
  ::close(STDIN_FILENO);

  int fd;

  {
    torrent::net::EventFd event;
    event.add_to_poll();
    fd = event.file_descriptor();

    event.remove_from_poll(torrent::this_thread::poll());
  }

  bool closed = ::fcntl(fd, F_GETFD) == -1;

  ::dup2(saved_stdin, STDIN_FILENO);
  ::close(saved_stdin);

  CPPUNIT_ASSERT_EQUAL(STDIN_FILENO, fd);
  CPPUNIT_ASSERT(closed);
}

#endif
