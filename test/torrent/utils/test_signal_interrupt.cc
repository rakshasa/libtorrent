#include "config.h"

#include "test_signal_interrupt.h"

// #include "helpers/test_thread.h"
// #include "helpers/test_utils.h"

#include "torrent/exceptions.h"
// #include "torrent/utils/signal_interrupt.h"
#include "torrent/utils/thread.h"
#include "torrent/utils/thread_interrupt.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestSignalInterrupt, "torrent/utils");

#include <sys/select.h>

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

void
TestSignalInterrupt::test_basic() {
  auto pair = torrent::thread_interrupt::create_pair();

  CPPUNIT_ASSERT(pair.first != nullptr);
  CPPUNIT_ASSERT(pair.second != nullptr);

  CPPUNIT_ASSERT(pair.first->file_descriptor() >= 0);
  CPPUNIT_ASSERT(pair.second->file_descriptor() >= 0);
  CPPUNIT_ASSERT(pair.first->file_descriptor() != pair.second->file_descriptor());

  for (int i = 0; i < 10; i++) {
    CPPUNIT_ASSERT(!pair.first->is_poking());
    CPPUNIT_ASSERT(!pair.second->is_poking());

    CPPUNIT_ASSERT(!check_event_is_readable(pair.second.get(), 0ms));
    CPPUNIT_ASSERT(!check_event_is_readable(pair.first.get(), 0ms));

    pair.first->poke();

    CPPUNIT_ASSERT(!pair.first->is_poking());
    CPPUNIT_ASSERT(pair.second->is_poking());

    CPPUNIT_ASSERT(check_event_is_readable(pair.second.get(), 1ms));
    CPPUNIT_ASSERT(!check_event_is_readable(pair.first.get(), 0ms));

    auto start_time = std::chrono::steady_clock::now();
    pair.second->event_read();
    auto end_time = std::chrono::steady_clock::now();

    CPPUNIT_ASSERT((end_time - start_time) < 1ms);
  }
}
