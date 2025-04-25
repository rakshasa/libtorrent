#ifndef TEST_HELPERS_TEST_MAIN_THREAD_H
#define TEST_HELPERS_TEST_MAIN_THREAD_H

#include <memory>

#include "test/helpers/test_thread.h"
#include "torrent/common.h"
#include "torrent/utils/thread.h"
#include "tracker/thread_tracker.h"

class TestMainThread : public torrent::utils::Thread {
public:
  static std::unique_ptr<TestMainThread> create();

  ~TestMainThread() override;

  const char*         name() const override  { return "rtorrent test main"; }

  void                init_thread() override;

private:
  TestMainThread();

  void                      call_events() override;
  std::chrono::microseconds next_timeout() override;
};

#define SETUP_THREAD_TRACKER()                                          \
  set_create_poll();                                                    \
  thread_management_type thread_management;                             \
  auto test_main_thread = TestMainThread::create();                     \
  test_main_thread->init_thread();                                      \
  torrent::ThreadTracker::create_thread(test_main_thread.get());        \
  torrent::thread_tracker()->init_thread();                             \
  torrent::thread_tracker()->start_thread();

// Make sure tearDown also calls torrent::ThreadTracker::destroy_thread().

#define CLEANUP_THREAD_TRACKER()                \
  torrent::ThreadTracker::destroy_thread();

#endif // TEST_HELPERS_TEST_MAIN_THREAD_H
