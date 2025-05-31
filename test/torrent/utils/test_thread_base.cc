#include "config.h"

#include "test_thread_base.h"

#include <functional>
#include <future>
#include <thread>
#include <unistd.h>

#include "helpers/test_thread.h"
#include "helpers/test_utils.h"
#include "torrent/exceptions.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_thread_base, "torrent/utils");

void
test_thread_base::test_basic() {
  auto thread = test_thread::create();

  CPPUNIT_ASSERT(thread->flags() == 0);

  CPPUNIT_ASSERT(!thread->is_active());
  CPPUNIT_ASSERT(thread->internal_poll() != nullptr);

  // Check active...
}

void
test_thread_base::test_lifecycle() {
  auto thread = test_thread::create();

  CPPUNIT_ASSERT(thread->state() == torrent::utils::Thread::STATE_UNKNOWN);
  CPPUNIT_ASSERT(thread->test_state() == test_thread::TEST_NONE);

  thread->init_thread();
  CPPUNIT_ASSERT(thread->state() == torrent::utils::Thread::STATE_INITIALIZED);
  CPPUNIT_ASSERT(thread->is_initialized());
  CPPUNIT_ASSERT(thread->test_state() == test_thread::TEST_PRE_START);

  thread->set_pre_stop();
  CPPUNIT_ASSERT(!wait_for_true(std::bind(&test_thread::is_test_state, thread.get(), test_thread::TEST_PRE_STOP)));

  thread->start_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread.get(), test_thread::STATE_ACTIVE)));
  CPPUNIT_ASSERT(thread->is_active());
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_test_state, thread.get(), test_thread::TEST_PRE_STOP)));

  thread->stop_thread_wait();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread.get(), test_thread::STATE_INACTIVE)));
  CPPUNIT_ASSERT(thread->is_inactive());
}

void
test_thread_base::test_interrupt() {
  auto thread = test_thread::create();

  thread->set_test_flag(test_thread::test_flag_long_timeout);

  thread->init_thread();
  thread->start_thread();

  // Vary the various timeouts.

  for (int i = 0; i < 100; i++) {
    thread->interrupt();
    usleep(0);

    thread->set_test_flag(test_thread::test_flag_do_work);
    thread->interrupt();

    // Wait for flag to clear.
    CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_not_test_flags, thread.get(), test_thread::test_flag_do_work)));
  }

  thread->stop_thread_wait();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread.get(), test_thread::STATE_INACTIVE)));
}

void
test_thread_base::test_stop() {
  for (int i = 0; i < 20; i++) {
    auto thread = test_thread::create();
    thread->set_test_flag(test_thread::test_flag_do_work);

    thread->init_thread();
    thread->start_thread();

    thread->stop_thread_wait();
    CPPUNIT_ASSERT(thread->is_inactive());
  }
}
