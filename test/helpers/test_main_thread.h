#ifndef TEST_HELPERS_TEST_MAIN_THREAD_H
#define TEST_HELPERS_TEST_MAIN_THREAD_H

#include "test/helpers/test_thread.h"
#include "torrent/common.h"
#include "torrent/utils/thread.h"
#include "tracker/thread_tracker.h"

class TestMainThread : public torrent::utils::Thread {
public:
  TestMainThread();
  ~TestMainThread() override;

  const char*         name() const override  { return "rtorrent test main"; }

  void                init_thread() override;

private:
  void                call_events() override;
  int64_t             next_timeout_usec() override;
};

#define SETUP_THREAD_TRACKER()                                          \
  set_create_poll();                                                    \
  thread_management_type thread_management;                             \
  auto test_main_thread = std::make_unique<TestMainThread>();           \
  test_main_thread->init_thread();                                      \
  torrent::thread_tracker = new torrent::ThreadTracker(torrent::thread_self()); \
  torrent::thread_tracker->init_thread();                               \
  torrent::thread_tracker->start_thread();

#define CLEANUP_THREAD_TRACKER()                                        \
  torrent::thread_tracker->stop_thread();                               \
  CPPUNIT_ASSERT(wait_for_true(std::bind(&torrent::utils::Thread::is_inactive, torrent::thread_tracker))); \
  delete torrent::thread_tracker;                                       \
  torrent::thread_tracker = nullptr;

#endif // TEST_HELPERS_TEST_MAIN_THREAD_H
