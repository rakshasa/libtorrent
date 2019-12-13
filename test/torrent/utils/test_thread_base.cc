#include "config.h"

#include "test_thread_base.h"

#include "helpers/test_thread.h"
#include "helpers/test_utils.h"

#include <functional>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/poll_select.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread_base.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_thread_base, "torrent/utils");

#define TEST_BEGIN(name)                                           \
  lt_log_print(torrent::LOG_MOCK_CALLS, "thread_base: %s", name);  \

void throw_shutdown_exception() { throw torrent::shutdown_exception(); }

void
test_thread_base::tearDown() {
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
  test_fixture::tearDown();
}

void
test_thread_base::test_basic() {
  test_thread* thread = new test_thread;

  CPPUNIT_ASSERT(thread->flags() == 0);

  CPPUNIT_ASSERT(!thread->is_main_polling());
  CPPUNIT_ASSERT(!thread->is_active());
  CPPUNIT_ASSERT(thread->global_queue_size() == 0);
  CPPUNIT_ASSERT(thread->poll() == NULL);

  // Check active...
}

void
test_thread_base::test_lifecycle() {
  test_thread* thread = new test_thread;

  CPPUNIT_ASSERT(thread->state() == torrent::thread_base::STATE_UNKNOWN);
  CPPUNIT_ASSERT(thread->test_state() == test_thread::TEST_NONE);

  thread->init_thread();
  CPPUNIT_ASSERT(thread->state() == torrent::thread_base::STATE_INITIALIZED);
  CPPUNIT_ASSERT(thread->is_initialized());
  CPPUNIT_ASSERT(thread->test_state() == test_thread::TEST_PRE_START);

  thread->set_pre_stop();
  CPPUNIT_ASSERT(!wait_for_true(std::bind(&test_thread::is_test_state, thread, test_thread::TEST_PRE_STOP)));

  thread->start_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread, test_thread::STATE_ACTIVE)));
  CPPUNIT_ASSERT(thread->is_active());
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_test_state, thread, test_thread::TEST_PRE_STOP)));

  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread, test_thread::STATE_INACTIVE)));
  CPPUNIT_ASSERT(thread->is_inactive());

  delete thread;
}

void
test_thread_base::test_global_lock_basic() {
  test_thread* thread = new test_thread;
  
  thread->init_thread();
  thread->start_thread();
  
  CPPUNIT_ASSERT(torrent::thread_base::global_queue_size() == 0);

  // Acquire main thread...
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());

  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());
    
  torrent::thread_base::release_global_lock();
  torrent::thread_base::acquire_global_lock();
  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());

  thread->set_acquire_global();
  CPPUNIT_ASSERT(!wait_for_true(std::bind(&test_thread::is_test_flags, thread, test_thread::test_flag_has_global)));
  
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_test_flags, thread, test_thread::test_flag_has_global)));

  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());

  // Test waive (loop).

  CPPUNIT_ASSERT(torrent::thread_base::global_queue_size() == 0);

  torrent::thread_base::release_global_lock();
  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread, test_thread::STATE_INACTIVE)));

  delete thread;
}

void
test_thread_base::test_interrupt() {
  test_thread* thread = new test_thread;
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
    CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_not_test_flags, thread, test_thread::test_flag_do_work)));
  }

  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread, test_thread::STATE_INACTIVE)));

  delete thread;
}

void
test_thread_base::test_stop() {
  { TEST_BEGIN("trylock global lock");
    CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
    // torrent::thread_base::acquire_global_lock();
  };

  for (int i = 0; i < 20; i++) {
    CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());

    test_thread* thread = new test_thread;
    thread->set_test_flag(test_thread::test_flag_do_work);

    { TEST_BEGIN("init and start thread");
      thread->init_thread();
      thread->start_thread();
    };

    { TEST_BEGIN("stop and delete thread");
      thread->stop_thread_wait();
      CPPUNIT_ASSERT(thread->is_inactive());

      delete thread;
    }
  }

  { TEST_BEGIN("release global lock");
    torrent::thread_base::release_global_lock();
  };
}
