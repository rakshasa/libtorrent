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

  thread_test();

  int                 test_state() const { return m_test_state; }
  bool                is_state(int state) const { return m_state == state; }
  bool                is_test_state(int state) const { return m_test_state == state; }

  void                init_thread();

  // static void         stop_thread(thread_test* thread) { } // Queue throw_shutdown_exception...

  void                set_pre_stop() { m_set_pre_stop = true; __sync_synchronize(); }
  void                set_shutdown() { m_set_shutdown = true; __sync_synchronize(); }

private:
  void                call_events();
  int64_t             next_timeout_usec() { return 100 * 1000; }

  int                 m_test_state;
  bool                m_set_pre_stop;
  bool                m_set_shutdown;
  bool                m_has_shutdown;
};

thread_test::thread_test() :
  m_test_state(TEST_NONE),
  m_set_pre_stop(false),
  m_set_shutdown(false),
  m_has_shutdown(false) {
}

void
thread_test::init_thread() {
  m_state = STATE_INITIALIZED;
  m_test_state = TEST_PRE_START;
  m_poll = torrent::PollSelect::create(256);
}

void
thread_test::call_events() {
  if (m_set_pre_stop && m_test_state == TEST_PRE_START)
    m_test_state = TEST_PRE_STOP;

  if (m_set_shutdown) {
    if (m_has_shutdown)
      throw torrent::internal_error("Already trigged shutdown.");
    
    m_has_shutdown = true;
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

// void
// utils_thread_base_test::test_global_lock...() {
// }
