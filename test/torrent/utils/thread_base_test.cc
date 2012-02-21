#include "config.h"

#include <unistd.h>
#include <tr1/functional>
#include <torrent/exceptions.h>
#include <torrent/poll_select.h>
#include <torrent/utils/thread_base.h>

#include "thread_base_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_thread_base_test);

namespace tr1 { using namespace std::tr1; }

void throw_shutdown_exception() { throw torrent::shutdown_exception(); }

thread_test::thread_test() :
  m_test_state(TEST_NONE),
  m_test_flags(0) {
}

void
thread_test::init_thread() {
  m_state = STATE_INITIALIZED;
  m_test_state = TEST_PRE_START;
  m_poll = torrent::PollSelect::create(256);
}

void
thread_test::call_events() {
  if ((m_test_flags & test_flag_pre_stop) && m_test_state == TEST_PRE_START && m_state == STATE_ACTIVE)
    __sync_lock_test_and_set(&m_test_state, TEST_PRE_STOP);

  if ((m_test_flags & test_flag_acquire_global)) {
    acquire_global_lock();
    __sync_and_and_fetch(&m_test_flags, ~test_flag_acquire_global);
    __sync_or_and_fetch(&m_test_flags, test_flag_has_global);
  }

  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw torrent::internal_error("Already trigged shutdown.");

    __sync_or_and_fetch(&m_flags, flag_did_shutdown);
    throw torrent::shutdown_exception();
  }

  if ((m_test_flags & test_flag_pre_poke)) {
  }

  if ((m_test_flags & test_flag_do_work)) {
    usleep(10 * 1000); // TODO: Don't just sleep, as that give up core.
    __sync_and_and_fetch(&m_test_flags, ~test_flag_do_work);
  }

  if ((m_test_flags & test_flag_post_poke)) {
  }
}

bool
wait_for_true(std::tr1::function<bool ()> test_function) {
  int i = 100;

  do {
    if (test_function())
      return true;

    usleep(10 * 1000);
  } while (--i);

  return false;
}

void
utils_thread_base_test::setUp() {
}

void
utils_thread_base_test::tearDown() {
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
}

void
utils_thread_base_test::test_basic() {
  thread_test* thread = new thread_test;

  CPPUNIT_ASSERT(thread->flags() == 0);

  CPPUNIT_ASSERT(!thread->is_main_polling());
  CPPUNIT_ASSERT(!thread->is_active());
  CPPUNIT_ASSERT(thread->global_queue_size() == 0);
  CPPUNIT_ASSERT(thread->poll() == NULL);

  // Check active...
}

void
utils_thread_base_test::test_lifecycle() {
  thread_test* thread = new thread_test;

  CPPUNIT_ASSERT(thread->state() == torrent::thread_base::STATE_UNKNOWN);
  CPPUNIT_ASSERT(thread->test_state() == thread_test::TEST_NONE);

  thread->init_thread();
  CPPUNIT_ASSERT(thread->state() == torrent::thread_base::STATE_INITIALIZED);
  CPPUNIT_ASSERT(thread->is_initialized());
  CPPUNIT_ASSERT(thread->test_state() == thread_test::TEST_PRE_START);

  thread->set_pre_stop();
  CPPUNIT_ASSERT(!wait_for_true(tr1::bind(&thread_test::is_test_state, thread, thread_test::TEST_PRE_STOP)));

  thread->start_thread();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_state, thread, thread_test::STATE_ACTIVE)));
  CPPUNIT_ASSERT(thread->is_active());
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_test_state, thread, thread_test::TEST_PRE_STOP)));

  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_state, thread, thread_test::STATE_INACTIVE)));
  CPPUNIT_ASSERT(thread->is_inactive());

  delete thread;
}

void
utils_thread_base_test::test_global_lock_basic() {
  thread_test* thread = new thread_test;
  
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
  CPPUNIT_ASSERT(!wait_for_true(tr1::bind(&thread_test::is_test_flags, thread, thread_test::test_flag_has_global)));
  
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_test_flags, thread, thread_test::test_flag_has_global)));

  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());

  // Test waive (loop).

  CPPUNIT_ASSERT(torrent::thread_base::global_queue_size() == 0);

  torrent::thread_base::release_global_lock();
  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_state, thread, thread_test::STATE_INACTIVE)));

  delete thread;
}

void
utils_thread_base_test::test_interrupt() {
  thread_test* thread = new thread_test;
  thread->set_test_flag(thread_test::test_flag_long_timeout);

  thread->init_thread();
  thread->start_thread();

  // Vary the various timeouts.

  for (int i = 0; i < 100; i++) {
    thread->interrupt();
    usleep(0);

    thread->set_test_flag(thread_test::test_flag_do_work);
    thread->interrupt();

    // Wait for flag to clear.
    CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_not_test_flags, thread, thread_test::test_flag_do_work)));
  }

  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_state, thread, thread_test::STATE_INACTIVE)));

  delete thread;
}

void
utils_thread_base_test::test_stop() {
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  // torrent::thread_base::acquire_global_lock();

  for (int i = 0; i < 20; i++) {
    CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());

    thread_test* thread = new thread_test;
    thread->set_test_flag(thread_test::test_flag_do_work);

    thread->init_thread();
    thread->start_thread();

    thread->stop_thread_wait();
    CPPUNIT_ASSERT(thread->is_inactive());

    delete thread;
  }

  torrent::thread_base::release_global_lock();
}
