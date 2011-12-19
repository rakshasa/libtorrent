#include "config.h"

#include <unistd.h>
#include <tr1/functional>
#include <torrent/exceptions.h>
#include <torrent/poll_select.h>
#include <torrent/utils/thread_base.h>

#include "thread_base_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_thread_base_test);

namespace std { using namespace tr1; }

void throw_shutdown_exception() { throw torrent::shutdown_exception(); }

class thread_test : public torrent::thread_base {
public:
  enum test_state {
    TEST_NONE,
    TEST_PRE_START,
    TEST_PRE_STOP,
    TEST_STOP
  };

  static const int test_flag_pre_stop     = 0x1;
  static const int test_flag_do_shutdown  = 0x2;
  static const int test_flag_has_shutdown = 0x4;

  static const int test_flag_acquire_global = 0x10;
  static const int test_flag_has_global     = 0x20;

  thread_test();

  int                 test_state() const { return m_test_state; }
  bool                is_state(int state) const { return m_state == state; }
  bool                is_test_state(int state) const { return m_test_state == state; }
  bool                is_test_flags(int flags) const { return m_test_flags == flags; }

  void                init_thread();

  void                set_pre_stop() { __sync_or_and_fetch(&m_test_flags, test_flag_pre_stop); }
  void                set_shutdown() { __sync_or_and_fetch(&m_test_flags, test_flag_do_shutdown); }

  void                set_acquire_global() { __sync_or_and_fetch(&m_test_flags, test_flag_acquire_global); }

private:
  void                call_events();
  int64_t             next_timeout_usec() { return 100 * 1000; }

  int                 m_test_state lt_cacheline_aligned;
  int                 m_test_flags lt_cacheline_aligned;
};

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

  if ((m_test_flags & test_flag_has_shutdown))
    throw torrent::internal_error("Already trigged shutdown.");

  if ((m_test_flags & test_flag_do_shutdown)) {
    __sync_or_and_fetch(&m_test_flags, test_flag_has_shutdown);
    throw torrent::shutdown_exception();
  }
}

bool
wait_for_true(std::tr1::function<bool ()> test_function) {
  int i = 10;

  do {
    if (test_function())
      return true;

    usleep(30 * 1000);
  } while (--i);

  return false;
}

void
utils_thread_base_test::setUp() {
}

void
utils_thread_base_test::tearDown() {
}

void
utils_thread_base_test::test_basic() {
  thread_test* thread = new thread_test;

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
  CPPUNIT_ASSERT(thread->test_state() == thread_test::TEST_PRE_START);

  thread->set_pre_stop();
  CPPUNIT_ASSERT(!wait_for_true(std::bind(&thread_test::is_test_state, thread, thread_test::TEST_PRE_STOP)));

  thread->start_thread();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&thread_test::is_state, thread, thread_test::STATE_ACTIVE)));
  CPPUNIT_ASSERT(wait_for_true(std::bind(&thread_test::is_test_state, thread, thread_test::TEST_PRE_STOP)));

  thread->set_shutdown();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&thread_test::is_state, thread, thread_test::STATE_INACTIVE)));

  delete thread;
}

void
utils_thread_base_test::test_global_lock_basic() {
  thread_test* thread = new thread_test;
  
  thread->init_thread();
  thread->start_thread();
  
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
  CPPUNIT_ASSERT(!wait_for_true(std::bind(&thread_test::is_test_flags, thread, thread_test::test_flag_has_global)));
  
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&thread_test::is_test_flags, thread, thread_test::test_flag_has_global)));

  CPPUNIT_ASSERT(!torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());

  // Test waive (loop).
}
